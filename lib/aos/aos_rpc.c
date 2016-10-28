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
//    /debug_printf("cb_accept_loop: invoked. sess=0x%08x\n", (int)cs);
    errval_t err;

    struct lmp_recv_msg message = LMP_RECV_MSG_INIT;
    struct capref received_cap=NULL_CAP;

    lmp_chan_recv(&cs->lc, &message, &received_cap);

    uint32_t return_opcode = RPC_NULL_OPCODE;
    uint32_t return_flags = RPC_FLAG_ERROR;
    bool send_ack = true;

    //debug_printf("We have message type: 0x%X\n", message.words[0]);
    uint32_t message_opcode=RPC_HEADER_OPCODE(message.words[0]);

    struct aos_rpc_message_handler_closure closure=cs->rpc->aos_rpc_message_handler_closure[message_opcode];
    struct capref ret_cap=NULL_CAP;

    if(closure.message_handler!=NULL){
        err=closure.message_handler(cs, &message, received_cap, &ret_cap, &return_opcode, &return_flags);
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
        //debug_printf("lmp_chan_send2 (0x%02x, 0x%02x)\n", return_opcode, return_flags);
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
void cb_recv_ready(void* args){
    struct aos_rpc_session *cs=args;

    struct lmp_recv_msg message = LMP_RECV_MSG_INIT;
    struct capref dummy_capref;

    lmp_chan_recv(&cs->lc, &message, &dummy_capref);

    if (RPC_HEADER_FLAGS(message.words[0]) & RPC_FLAG_ACK){
        cs->ack_received=true;
    }
}

static
void cb_send_ready(void* args){
    struct aos_rpc_session *sess = args;
    sess->can_send=true;
}

static
void wait_for_send(struct aos_rpc_session* sess){
    errval_t err;

    lmp_chan_register_send(&sess->lc, sess->rpc->ws, MKCLOSURE(cb_send_ready, (void*)sess));

    while (!sess->can_send) {
        err = event_dispatch(sess->rpc->ws);
        if (err_is_fail(err)) {
            DEBUG_ERR(err, "in event_dispatch");
            abort();
        }
    }
    sess->can_send=false;
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

static
void wait_for_ack(struct aos_rpc_session* sess){
    lmp_chan_register_recv(&sess->lc,
            sess->rpc->ws, MKCLOSURE(cb_recv_ready, (void*)sess));

    while (!sess->ack_received) {
        errval_t err = event_dispatch(sess->rpc->ws);
        if (err_is_fail(err)) {
            DEBUG_ERR(err, "in event_dispatch");
            abort();
        }
    }
    sess->ack_received=false;
}

// Waits for send, sends and waits for ack
#define RPC_CHAN_WRAPPER_SEND(rpc, call) \
    { \
		reset_cycle_counter(); \
        assert(rpc->server_sess); \
        wait_for_send(rpc->server_sess); \
        errval_t _err = call; \
        if (err_is_fail(_err)) \
            DEBUG_ERR(_err, "Send failed in " __FILE__ ":%d", __LINE__); \
        wait_for_ack(rpc->server_sess); \
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
    // we can only send strings of up to 8 characters, need to do the stub
    errval_t err;

    size_t length=strlen(string);
    length++;
    size_t curr_position=0;
        intptr_t buffer[8];

    uint32_t flags = RPG_FLAG_NONE;
    do{
        memcpy(buffer, string+curr_position, (length>LMP_MAX_BUFF_SIZE)?LMP_MAX_BUFF_SIZE:length);

        if (length>LMP_MAX_BUFF_SIZE){
            curr_position+=LMP_MAX_BUFF_SIZE;
            length-=LMP_MAX_BUFF_SIZE;
            flags = RPC_FLAG_INCOMPLETE;
        }else{
            flags = RPG_FLAG_NONE;
            curr_position+=length;
        }

        wait_for_send(rpc->server_sess);

        err=lmp_ep_send(rpc->server_sess->lc.remote_cap,
            LMP_FLAG_SYNC,
            NULL_CAP,
            9,
            MAKE_RPC_MSG_HEADER(RPC_STRING, flags),
                buffer[0], buffer[1], buffer[2], buffer[3],
                buffer[4], buffer[5], buffer[6], buffer[7]);

        if(err_is_fail(err)) {
            DEBUG_ERR(err, "sending string");
        }
        wait_for_ack(rpc->server_sess);

    } while (flags);

    return SYS_ERR_OK;
}

errval_t aos_rpc_get_ram_cap(struct aos_rpc *rpc,
    size_t request_bits,
    size_t alignment,
    struct capref *retcap,
    size_t *ret_bits)
{
    wait_for_send(rpc->server_sess);
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
    wait_for_send(rpc->server_sess);
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
    // TODO (milestone 5): implement spawn new process rpc
    return SYS_ERR_OK;
}

errval_t aos_rpc_process_get_name(struct aos_rpc *rpc, domainid_t pid,
                                  char **name)
{
    // TODO (milestone 5): implement name lookup for process given a process
    // id
    return SYS_ERR_OK;
}

errval_t aos_rpc_process_get_all_pids(struct aos_rpc *rpc,
                                      domainid_t **pids, size_t *pid_count)
{
    // TODO (milestone 5): implement process id discovery
    return SYS_ERR_OK;
}

errval_t aos_rpc_register_handler(struct aos_rpc* rpc, enum message_opcodes opcode,
        aos_rpc_handler message_handler, bool send_ack){

    rpc->aos_rpc_message_handler_closure[opcode].send_ack=send_ack;
    rpc->aos_rpc_message_handler_closure[opcode].message_handler=message_handler;

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
    RPC_CHAN_WRAPPER_SEND(rpc,
        lmp_chan_send1(&rpc->server_sess->lc,
            LMP_FLAG_SYNC,
            selfep,
            RPC_HANDSHAKE));
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
        rpc->server_sess = malloc(sizeof(struct aos_rpc_session));
        // Create chan to server
        ERROR_RET1(lmp_chan_accept(&rpc->server_sess->lc,
                DEFAULT_LMP_BUF_WORDS, remote_endpoint));
        rpc->server_sess->ack_received=false;
        rpc->server_sess->can_send=false;
        rpc->server_sess->rpc = rpc;
        ERROR_RET1(lmp_chan_alloc_recv_slot(&rpc->server_sess->lc));

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

errval_t aos_server_add_client(struct aos_rpc* rpc, struct aos_rpc_session** sess)
{
    // TODO: Free this when process ends
    *sess = malloc(sizeof(struct aos_rpc_session));
    (*sess)->ack_received=false;
    (*sess)->can_send=false;
    (*sess)->rpc = rpc;
    ERROR_RET1(lmp_chan_accept(&(*sess)->lc,
            DEFAULT_LMP_BUF_WORDS,
            NULL_CAP));
    ERROR_RET1(lmp_chan_alloc_recv_slot(&(*sess)->lc));
    return SYS_ERR_OK;
}

errval_t aos_server_register_client(struct aos_rpc* rpc, struct aos_rpc_session* sess)
{
    ERROR_RET1(lmp_chan_register_recv(&sess->lc,
        rpc->ws, MKCLOSURE(cb_accept_loop, sess)));
    return SYS_ERR_OK;
}
