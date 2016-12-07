/**
 * \file
 * \brief Implementation of AOS rpc-like messaging
 */

/*
 * Copyright (c) 2013 - 2016, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetstr. 6, CH-8092 Zurich. Attn: Systems Group.
 */

#include <aos/aos_rpc.h>
#include <arch/arm/barrelfish_kpi/asm_inlines_arch.h>

/*
*   // does the sender want to yield their timeslice on success?
    bool sync = flags & LMP_FLAG_SYNC;
    // does the sender want to yield to the target
    // if undeliverable?
    bool yield = flags & LMP_FLAG_YIELD;
    // is the cap (if present) to be deleted on send?
    bool give_away = flags & LMP_FLAG_GIVEAWAY;
 */

const size_t LMP_MAX_BUFF_SIZE=8*sizeof(uintptr_t);

static
void cb_accept_loop(void* args)
{
    struct aos_rpc_session* cs=(struct aos_rpc_session*)args;
    errval_t err;

    struct lmp_recv_msg message = LMP_RECV_MSG_INIT;
    struct capref received_cap=NULL_CAP;

    lmp_chan_recv(&cs->lc, &message, &received_cap);

    uint32_t return_opcode = RPC_NULL_OPCODE;
    uint32_t return_flags = RPC_FLAG_ERROR;
    bool send_ack = true;

    uint32_t message_opcode=RPC_HEADER_OPCODE(message.words[0]);

    struct aos_rpc_message_handler_closure closure=cs->rpc->aos_rpc_message_handler_closure[message_opcode];
    struct capref ret_cap=NULL_CAP;

    if(closure.message_handler!=NULL){
        err=closure.message_handler(cs, &message, received_cap, closure.args, &ret_cap, &return_opcode, &return_flags);
        if (!closure.send_ack && !err_is_fail(err))
            send_ack = false;

        if(err_is_fail(err))
            return_flags = RPC_FLAG_ERROR;
    }else{
        debug_printf("Callback not registered, sending error\n");

        err = RPC_ERR_SERVICE_NOT_FOUND;
        return_flags = RPC_FLAG_ERROR;

        //TODO: if someone sent cap, we have to free it
    }

    if (send_ack)
    {
        return_flags |= RPC_FLAG_ACK;
        err = lmp_chan_send2(&cs->lc,
            LMP_FLAG_SYNC,
            ret_cap,
            MAKE_RPC_MSG_HEADER(return_opcode, return_flags),
            err);

        if (err_is_fail(err))
            debug_printf("Response message not sent\n");
    }

    if(!capcmp(received_cap, NULL_CAP)){
        debug_printf("Capabilities changed, allocating new slot\n");
        lmp_chan_alloc_recv_slot(&cs->lc);
    }
    lmp_chan_register_recv(&cs->lc, cs->rpc->ws, MKCLOSURE(cb_accept_loop, args));
}

static
void cb_send_ready(void* args){
    struct aos_rpc_session *sess = args;
    sess->can_send=true;
}

static
errval_t wait_for_send(struct aos_rpc_session* sess)
{
    ERROR_RET2(lmp_chan_register_send(&sess->lc, sess->rpc->ws,
        MKCLOSURE(cb_send_ready, (void*)sess)),
        RPC_ERR_WAIT_SEND);

    while (!sess->can_send)
        ERROR_RET2(event_dispatch(sess->rpc->ws),
            RPC_ERR_WAIT_SEND);
    sess->can_send = false;
    return SYS_ERR_OK;
}

struct recv_block_helper_struct
{
    bool received;
    errval_t err;
    struct lmp_recv_msg* message;
    struct capref* cap;
    struct aos_rpc_session* sess;
};

static void cb_recv_first(void* args)
{
    //debug_printf("cb_recv_first\n");
    struct recv_block_helper_struct *rb = args;
    rb->err = lmp_chan_recv(&rb->sess->lc, rb->message, rb->cap);
    // Re-register if fails
    if (err_is_fail(rb->err) && lmp_err_is_transient(rb->err))
    {
        lmp_chan_register_recv(&rb->sess->lc, rb->sess->rpc->ws,
            MKCLOSURE(cb_recv_first, args));
        return;
    }
    rb->received = true;
}

errval_t recv_block(struct aos_rpc_session* sess,
    struct lmp_recv_msg* message,
    struct capref* cap)
{
    //debug_printf("recv_block\n");
    if (!message || !cap)
        return RPC_ERR_INVALID_ARGUMENTS;

    message->buf.buflen = LMP_MSG_LENGTH;

    struct recv_block_helper_struct rb;
    rb.received = false;
    rb.message = message;
    rb.sess = sess;
    rb.cap = cap;

    ERROR_RET1(lmp_chan_register_recv(&sess->lc,
            sess->rpc->ws,
            MKCLOSURE(cb_recv_first, (void*)&rb)));

    while (!rb.received)
        ERROR_RET1(event_dispatch(sess->rpc->ws));

    if (err_is_fail(rb.err))
        return rb.err;

    if (!capcmp(*rb.cap, NULL_CAP))
        ERROR_RET1(lmp_chan_alloc_recv_slot(&sess->lc));

    if (RPC_HEADER_FLAGS(rb.message->words[0]) & RPC_FLAG_ERROR)
        return rb.message->words[1];

    return SYS_ERR_OK;
}

errval_t wait_for_ack_with_message(struct aos_rpc_session* sess, struct lmp_recv_msg* message, struct capref* ret_capref)
{
    ERROR_RET1(recv_block(sess, message, ret_capref));

    if (RPC_HEADER_FLAGS(message->words[0]) & RPC_FLAG_ACK)
        return SYS_ERR_OK;
    return RPC_ERR_INVALID_PROTOCOL;
}

static
errval_t wait_for_ack(struct aos_rpc_session* sess)
{
    struct lmp_recv_msg message = LMP_RECV_MSG_INIT;
    struct capref dummy_capref;

    return wait_for_ack_with_message(sess, &message, &dummy_capref);
}

// Waits for send, sends and waits for ack
#define RPC_CHAN_WRAPPER_SEND(rpc, call) \
    { \
        reset_cycle_counter(); \
        assert(rpc->server_sess); \
        ERROR_RET1(wait_for_send(rpc->server_sess)); \
        errval_t _err = call; \
        if (err_is_fail(_err)) \
            DEBUG_ERR(_err, "Send failed in " __FILE__ ":%d", __LINE__); \
        ERROR_RET1(wait_for_ack(rpc->server_sess)); \
    }

#define RPC_CHAN_WRAPPER_SEND_WITH_MESSAGE_RESPONSE(rpc, call, message, retcap) \
    { \
        reset_cycle_counter(); \
        assert(rpc->server_sess); \
        ERROR_RET1(wait_for_send(rpc->server_sess)); \
        errval_t _err = call; \
        if (err_is_fail(_err)) \
            DEBUG_ERR(_err, "Send failed in " __FILE__ ":%d", __LINE__); \
        ERROR_RET1(wait_for_ack_with_message(rpc->server_sess, message, retcap)); \
    }

/*
 * sends a number over the channel
 */
errval_t aos_rpc_send_number(struct aos_rpc *rpc, uintptr_t val)
{
    RPC_CHAN_WRAPPER_SEND(rpc,
        lmp_chan_send2(&rpc->server_sess->lc,
            LMP_FLAG_SYNC,
            NULL_CAP,
            RPC_NUMBER,
            val));
    return SYS_ERR_OK;
}

errval_t aos_rpc_send_string(struct aos_rpc *rpc, const char *string)
{
    assert(rpc->server_sess);
    size_t size = strlen(string);
    if (size > rpc->server_sess->shared_buffer_size)
        return RPC_ERR_BUF_TOO_SMALL;

    memcpy(rpc->server_sess->shared_buffer, string, size);

    RPC_CHAN_WRAPPER_SEND(rpc,
        lmp_chan_send2(&rpc->server_sess->lc,
            LMP_FLAG_SYNC,
            NULL_CAP,
            RPC_STRING,
            size));
    return SYS_ERR_OK;
}

errval_t aos_rpc_create_server_socket(struct aos_rpc *rpc, struct capref shared_buffer, size_t port){
    assert(rpc->server_sess);

    RPC_CHAN_WRAPPER_SEND(rpc,
    lmp_chan_send2(&rpc->server_sess->lc,
       LMP_FLAG_SYNC,
       shared_buffer,
       RPC_CREATE_SERVER_SOCKET,
       port
     ));

    return SYS_ERR_OK;
}

errval_t aos_connect_to_port(struct aos_rpc *rpc,
    uint32_t port,
    struct capref *retcap)
{
    ERROR_RET1(wait_for_send(rpc->server_sess));
    ERROR_RET1(lmp_chan_send2(&rpc->server_sess->lc,
            LMP_FLAG_SYNC,
            NULL_CAP,
            RPC_CONNECT_TO_SOCKET,
            port));
    struct lmp_recv_msg message=LMP_RECV_MSG_INIT;
    ERROR_RET1(recv_block(rpc->server_sess, &message, retcap));
    ASSERT_PROTOCOL(RPC_HEADER_OPCODE(message.words[0]) == RPC_RAM_CAP_RESPONSE);

    return SYS_ERR_OK;
}

errval_t aos_rpc_get_special_capability(struct aos_rpc *rpc, enum aos_rpc_cap_type cap_type,
        struct capref *retcap){

    ERROR_RET1(wait_for_send(rpc->server_sess));
    ERROR_RET1(lmp_chan_send2(&rpc->server_sess->lc,
            LMP_FLAG_SYNC,
            NULL_CAP,
            RPC_SPECIAL_CAP_QUERY,
            (uint32_t)cap_type));
    struct lmp_recv_msg message=LMP_RECV_MSG_INIT;
    ERROR_RET1(recv_block(rpc->server_sess, &message, retcap));
    ASSERT_PROTOCOL(RPC_HEADER_OPCODE(message.words[0]) == RPC_SPECIAL_CAP_RESPONSE);
    return SYS_ERR_OK;
}

errval_t aos_rpc_get_ram_cap(struct aos_rpc *rpc,
    size_t request_bits,
    size_t alignment,
    struct capref *retcap,
    size_t *ret_bits)
{
    ERROR_RET1(wait_for_send(rpc->server_sess));
    ERROR_RET1(lmp_chan_send3(&rpc->server_sess->lc,
            LMP_FLAG_SYNC,
            NULL_CAP,
            RPC_RAM_CAP_QUERY,
            request_bits, alignment));
    struct lmp_recv_msg message=LMP_RECV_MSG_INIT;
    ERROR_RET1(recv_block(rpc->server_sess, &message, retcap));
    ASSERT_PROTOCOL(RPC_HEADER_OPCODE(message.words[0]) == RPC_RAM_CAP_RESPONSE);
    *ret_bits = message.words[1];
    return SYS_ERR_OK;
}

errval_t aos_rpc_serial_getchar(struct aos_rpc *rpc, char *retc)
{
    // TODO implement functionality to request a character from
    // the serial driver.
    ERROR_RET1(wait_for_send(rpc->server_sess));
    ERROR_RET1(lmp_chan_send1(&rpc->server_sess->lc,
            LMP_FLAG_SYNC,
            NULL_CAP,
            RPC_GET_CHAR));
    struct lmp_recv_msg message=LMP_RECV_MSG_INIT;
    struct capref tmp_cap;
    ERROR_RET1(recv_block(rpc->server_sess, &message, &tmp_cap));
    ASSERT_PROTOCOL(RPC_HEADER_OPCODE(message.words[0]) == RPC_GET_CHAR);
    *retc=*(char*)(message.words+1);

    return SYS_ERR_OK;
}

/*
 * Sends a character to the serial port.
 */
errval_t aos_rpc_serial_putchar(struct aos_rpc *rpc, char c)
{
    RPC_CHAN_WRAPPER_SEND(rpc,
        lmp_chan_send2(&rpc->server_sess->lc,
            LMP_FLAG_SYNC,
            NULL_CAP,
            RPC_PUT_CHAR,
            c));
    return SYS_ERR_OK;
}

errval_t aos_rpc_process_spawn(struct aos_rpc *rpc, char *name,
        coreid_t core, domainid_t *newpid)
{
    assert(rpc->server_sess);
    size_t size = strlen(name);

    if (size > rpc->server_sess->shared_buffer_size)
        return RPC_ERR_BUF_TOO_SMALL;

    memcpy(rpc->server_sess->shared_buffer, name, size);

    ERROR_RET1(wait_for_send(rpc->server_sess));
    ERROR_RET1(lmp_chan_send3(&rpc->server_sess->lc,
            LMP_FLAG_SYNC,
            NULL_CAP,
            RPC_SPAWN,
            size,
            core));

    struct lmp_recv_msg message=LMP_RECV_MSG_INIT;
    struct capref tmp_cap;
    ERROR_RET1(recv_block(rpc->server_sess, &message, &tmp_cap));
    ASSERT_PROTOCOL(RPC_HEADER_OPCODE(message.words[0]) == RPC_SPAWN);

    if (RPC_HEADER_FLAGS(message.words[0]) & RPC_FLAG_ERROR) {
        *newpid = 0;
        return SPAWN_ERR_DOMAIN_NOTFOUND;
    }

    *newpid = message.words[1];
    return SYS_ERR_OK;
}

errval_t aos_rpc_process_exit(struct aos_rpc *rpc) {
    assert(rpc->server_sess);

    debug_printf("Sending exit message\n");

    ERROR_RET1(wait_for_send(rpc->server_sess));
    ERROR_RET1(lmp_chan_send1(&rpc->server_sess->lc,
            LMP_FLAG_SYNC,
            NULL_CAP,
            RPC_EXIT));

    return SYS_ERR_OK;
}

errval_t aos_rpc_process_get_name(struct aos_rpc *rpc, domainid_t pid,
                                  char **name)
{
    // send RPC_GET_NAME message to server containing PID
    ERROR_RET1(wait_for_send(rpc->server_sess));
    ERROR_RET1(lmp_chan_send2(&rpc->server_sess->lc,
            LMP_FLAG_SYNC,
            NULL_CAP,
            RPC_GET_NAME,
            pid));

    // expect a response with header RPC_GET_NAME
    struct lmp_recv_msg message=LMP_RECV_MSG_INIT;
    struct capref tmp_cap;
    ERROR_RET1(recv_block(rpc->server_sess, &message, &tmp_cap));
    ASSERT_PROTOCOL(RPC_HEADER_OPCODE(message.words[0]) == RPC_GET_NAME);

    // if we get an error, it's because the pid was not found
    if (RPC_HEADER_FLAGS(message.words[0]) & RPC_FLAG_ERROR) {
        **name = 0;
        return SPAWN_ERR_DOMAIN_NOTFOUND;
    }

    // read the name from the shared buffer into the return argument
    if (!rpc->server_sess->shared_buffer_size) {
        return RPC_ERR_SHARED_BUF_EMPTY;
    }

    size_t string_size = message.words[1];
    ASSERT_PROTOCOL(string_size <= rpc->server_sess->shared_buffer_size);

    *name = malloc(string_size+1);
    memcpy(*name, rpc->server_sess->shared_buffer, string_size);
    (*name)[string_size] = 0;

    return SYS_ERR_OK;
}

errval_t aos_rpc_process_get_all_pids(struct aos_rpc *rpc,
        domainid_t **pids, size_t *pid_count)
{
    ERROR_RET1(wait_for_send(rpc->server_sess));
    ERROR_RET1(lmp_chan_send1(&rpc->server_sess->lc,
            LMP_FLAG_SYNC,
            NULL_CAP,
            RPC_GET_PID));

    struct lmp_recv_msg message=LMP_RECV_MSG_INIT;
    struct capref tmp_cap;
    ERROR_RET1(recv_block(rpc->server_sess, &message, &tmp_cap));
    ASSERT_PROTOCOL(RPC_HEADER_OPCODE(message.words[0]) == RPC_GET_PID);

    // read the array of pids from the shared buffer into the return argument
    if (!rpc->server_sess->shared_buffer_size)
        return RPC_ERR_SHARED_BUF_EMPTY;

    *pid_count = message.words[1];
    ASSERT_PROTOCOL(*pid_count <= rpc->server_sess->shared_buffer_size);

    *pids = malloc(sizeof(domainid_t) * (*pid_count));
    memcpy(*pids, rpc->server_sess->shared_buffer, *pid_count * sizeof(domainid_t));

    return SYS_ERR_OK;
}

errval_t aos_rpc_register_handler(struct aos_rpc* rpc, enum message_opcodes opcode,
        aos_rpc_handler message_handler, bool send_ack){

    rpc->aos_rpc_message_handler_closure[opcode].send_ack=send_ack;
    rpc->aos_rpc_message_handler_closure[opcode].message_handler=message_handler;
    rpc->aos_rpc_message_handler_closure[opcode].args=NULL;

    return SYS_ERR_OK;
}

errval_t aos_rpc_register_handler_with_context(struct aos_rpc* rpc, enum message_opcodes opcode,
        aos_rpc_handler message_handler, bool send_ack, void* context){

    rpc->aos_rpc_message_handler_closure[opcode].send_ack=send_ack;
    rpc->aos_rpc_message_handler_closure[opcode].message_handler=message_handler;
    rpc->aos_rpc_message_handler_closure[opcode].args=context;

    return SYS_ERR_OK;
}

errval_t aos_rpc_accept(struct aos_rpc* rpc){
    errval_t err;

    while (true) {
        err = event_dispatch(rpc->ws);
        if (err_is_fail(err)) {
            DEBUG_ERR(err, "in event_dispatch");
            abort();
        }
    }
}

static
errval_t aos_rpc_send_handshake(struct aos_rpc *rpc, struct capref selfep)
{
    // Client side
    RPC_CHAN_WRAPPER_SEND(rpc,
        lmp_chan_send1(&rpc->server_sess->lc,
            LMP_FLAG_SYNC,
            selfep,
            RPC_HANDSHAKE));
    return aos_rpc_request_shared_buffer(rpc, 100);
}

errval_t aos_rpc_request_shared_buffer(struct aos_rpc* rpc, size_t size)
{
    if (!rpc->server_sess)
        return RPC_ERR_INVALID_ARGUMENTS;

    size = ROUND_UP(size, BASE_PAGE_SIZE);

    // Request shared buffer
    rpc->server_sess->shared_buffer_size = 0; // Disable buffer
    ERROR_RET1(wait_for_send(rpc->server_sess));
    ERROR_RET1(lmp_chan_send2(&rpc->server_sess->lc,
        LMP_FLAG_SYNC,
        NULL_CAP,
        RPC_SHARED_BUFFER_REQUEST,
        size));
    struct lmp_recv_msg message;
    ERROR_RET1(recv_block(rpc->server_sess,
        &message,
        &rpc->server_sess->shared_buffer_cap));

    // Setup buffer
    return aos_rpc_map_shared_buffer(rpc->server_sess, size);
}

errval_t aos_rpc_map_shared_buffer(struct aos_rpc_session* sess, size_t size)
{
    struct paging_state* ps = get_current_paging_state();
    ERROR_RET1(paging_map_frame(ps, &sess->shared_buffer,
        size,
        sess->shared_buffer_cap,
        NULL, NULL));

    sess->shared_buffer_size = size;
    return SYS_ERR_OK;
}

static inline
errval_t aos_rpc_session_init(struct aos_rpc_session* sess,
    struct capref remote_endpoint)
{
    ERROR_RET1(lmp_chan_accept(&sess->lc,
            DEFAULT_LMP_BUF_WORDS, remote_endpoint));
    sess->ack_received=false;
    sess->can_send=false;
    sess->shared_buffer_size = 0; // Disable buffer
    ERROR_RET1(slot_alloc(&sess->shared_buffer_cap));
    ERROR_RET1(lmp_chan_alloc_recv_slot(&sess->lc));
    return SYS_ERR_OK;
}

errval_t aos_rpc_init(struct aos_rpc *rpc, struct capref remote_endpoint, bool is_client)
{
    debug_printf("aos_rpc_init: Created for %s\n", is_client ? "CLIENT" : "SERVER");
    rpc->ws = get_default_waitset();
    memset(rpc->aos_rpc_message_handler_closure,
        0, sizeof(rpc->aos_rpc_message_handler_closure));

    if (is_client)
    {
        // Create chan to server
        rpc->server_sess = malloc(sizeof(struct aos_rpc_session));
        rpc->server_sess->rpc = rpc;
        ERROR_RET1(aos_rpc_session_init(rpc->server_sess, remote_endpoint));

        debug_printf("Sending handshake\n");
        ERROR_RET1(aos_rpc_send_handshake(rpc,
            rpc->server_sess->lc.local_cap));

        // store it at a well known location
        set_init_rpc(rpc);
    }
    else
        rpc->server_sess = NULL;

    return SYS_ERR_OK;
}

errval_t aos_rpc_udp_connect(struct aos_rpc *rpc, struct capref urpc_frame, uint32_t address, uint16_t port){
    RPC_CHAN_WRAPPER_SEND(rpc,
        lmp_chan_send3(&rpc->server_sess->lc,
            LMP_FLAG_SYNC,
            urpc_frame,
            RPC_NETWORK_UDP_CONNECT,
            address,
            port));

    return SYS_ERR_OK;
}

errval_t aos_rpc_udp_create_server(struct aos_rpc *rpc, struct capref urpc_frame, uint16_t port){
    RPC_CHAN_WRAPPER_SEND(rpc,
        lmp_chan_send2(&rpc->server_sess->lc,
            LMP_FLAG_SYNC,
            urpc_frame,
            RPC_NETWORK_UDP_CREATE_SERVER,
            port));

    return SYS_ERR_OK;
}


errval_t aos_server_add_client(struct aos_rpc* rpc, struct aos_rpc_session** sess)
{
    // TODO: Free this when process ends
    *sess = malloc(sizeof(struct aos_rpc_session));
    (*sess)->rpc = rpc;
    ERROR_RET1(aos_rpc_session_init(*sess, NULL_CAP));
    return SYS_ERR_OK;
}

errval_t aos_rpc_get_device_cap(struct aos_rpc *rpc,
                                lpaddr_t paddr, size_t bytes,
                                struct capref *frame)
{
    return LIB_ERR_NOT_IMPLEMENTED;
}

errval_t aos_server_register_client(struct aos_rpc* rpc, struct aos_rpc_session* sess)
{
    ERROR_RET1(lmp_chan_register_recv(&sess->lc,
        rpc->ws, MKCLOSURE(cb_accept_loop, sess)));
    return SYS_ERR_OK;
}
