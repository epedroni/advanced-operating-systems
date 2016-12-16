#include "lrpc_server.h"
#include "init.h"
#include "nameserver.h"
#include <arch/arm/barrelfish_kpi/asm_inlines_arch.h>
#include <omap44xx_map.h>
#include <aos/urpc/udp.h>
#include <aos/paging.h>

#define DEBUG_LRPC(s, ...) //debug_printf("[RPC] " s "\n", ##__VA_ARGS__)

struct lmp_chan* networking_lmp_chan;

static
void cb_send_ready(void* args){
    struct aos_rpc_session *sess = args;
    sess->can_send=true;
}

static
errval_t wait_for_send(struct lmp_chan* lc, struct aos_rpc_session* sess)
{
    ERROR_RET2(lmp_chan_register_send(lc, sess->rpc->ws,
        MKCLOSURE(cb_send_ready, (void*)sess)),
        RPC_ERR_WAIT_SEND);

    while (!sess->can_send)
        ERROR_RET2(event_dispatch(sess->rpc->ws),
            RPC_ERR_WAIT_SEND);
    sess->can_send = false;
    return SYS_ERR_OK;
}

static
errval_t handle_nameserver_cap(struct aos_rpc_session* sess,
        struct lmp_recv_msg* msg,
        struct capref received_capref,
        void* context,
        struct capref* ret_cap,
        uint32_t* ret_type,
        uint32_t* ret_flags)
{
    DEBUG_LRPC("Recv RPC_NAMESERVER_CAP", 0);
    // XXX DEBUG
    struct capability selfep, nsep;
    debug_cap_identify(cap_nameserverep, &nsep);
    debug_cap_identify(cap_selfep, &selfep);

    if (nsep.u.endpoint.listener == selfep.u.endpoint.listener
                && nsep.u.endpoint.epoffset == selfep.u.endpoint.epoffset) {
        // if the nameserver isnt done, this is it trying to register
    	DEBUG_LRPC("Try deleting the cap first\n");
        cap_delete(cap_nameserverep);
        DEBUG_LRPC("Now copy\n");
        cap_copy(cap_nameserverep, received_capref);
    }
    return SYS_ERR_OK;
}

static
errval_t handle_handshake(struct aos_rpc_session* sess,
        struct lmp_recv_msg* msg,
        struct capref received_capref,
        void* context,
        struct capref* ret_cap,
        uint32_t* ret_type,
        uint32_t* ret_flags)
{
    DEBUG_LRPC("Recv RPC_HANDSHAKE", 0);
    sess->lc.remote_cap=received_capref;
    return SYS_ERR_OK;
}

static
errval_t handle_shared_buffer_request(struct aos_rpc_session* sess,
        struct lmp_recv_msg* msg,
        struct capref received_capref,
        void* context,
        struct capref* ret_cap,
        uint32_t* ret_type,
        uint32_t* ret_flags)
{
    size_t request_size = msg->words[1];
	struct paging_state* ps = get_current_paging_state();
    DEBUG_LRPC("Recv RPC_SHARED_BUFFER_REQUEST [size 0x%x]", request_size);

    // 1. Free current buffer
    if (sess->shared_buffer_size)
    {
        sess->shared_buffer_size = 0;
        ERROR_RET1(paging_unmap(ps, sess->shared_buffer));
        // TODO: Free ram? How? We may not be in the RAM server...
        ERROR_RET1(cap_destroy(sess->shared_buffer_cap));
    }

    // 2. Allocate & map requested size
    struct capref ram_cap;
    ERROR_RET1(ram_alloc(&ram_cap, request_size));
    ERROR_RET1(cap_retype(sess->shared_buffer_cap,
        ram_cap,
        0, ObjType_Frame, request_size, 1));
    ERROR_RET1(aos_rpc_map_shared_buffer(sess, request_size));
    sess->shared_buffer_size = request_size;

    // 3. Send back cap
    *ret_cap = sess->shared_buffer_cap;

    return SYS_ERR_OK;
}

static
errval_t handle_number(struct aos_rpc_session* sess,
        struct lmp_recv_msg* msg,
        struct capref received_capref,
        void* context,
        struct capref* ret_cap,
        uint32_t* ret_type,
        uint32_t* ret_flags)
{
    DEBUG_LRPC("Received number %d, it took %d cycles",msg->words[1], get_cycle_count());
    return SYS_ERR_OK;
}

static
errval_t handle_string(struct aos_rpc_session* sess,
        struct lmp_recv_msg* msg,
        struct capref received_capref,
        void* context,
        struct capref* ret_cap,
        uint32_t* ret_type,
        uint32_t* ret_flags)
{
    if (!sess->shared_buffer_size)
        return RPC_ERR_SHARED_BUF_EMPTY;

    size_t string_size = msg->words[1];
    ASSERT_PROTOCOL(string_size <= sess->shared_buffer_size);

    DEBUG_LRPC("Recv RPC_STRING [string size %d]\n", string_size);
    sys_print(sess->shared_buffer, string_size);
    sys_print("\n", 1);
    return SYS_ERR_OK;
}

static
errval_t handle_ram_cap_opcode(struct aos_rpc_session* sess,
        struct lmp_recv_msg* msg,
        struct capref received_capref,
        void* context,
        struct capref* ret_cap,
        uint32_t* ret_type,
        uint32_t* ret_flags)
{
    size_t requested_bytes = msg->words[1];
    size_t requested_aligment = msg->words[2];
    DEBUG_LRPC("Recvd RPC_RAM_CAP_QUERY [requested %d bytes | aligned 0x%x]",
        (int)requested_bytes, (int)requested_aligment);

    struct capref return_cap;
    ERROR_RET1(ram_alloc_aligned(&return_cap, requested_bytes, requested_aligment));
    ERROR_RET1(lmp_chan_send2(&sess->lc,
        LMP_FLAG_SYNC,
        return_cap,
        MAKE_RPC_MSG_HEADER(RPC_RAM_CAP_RESPONSE, RPC_FLAG_ACK),
        requested_bytes));
    return SYS_ERR_OK;
}

static
errval_t handle_get_special_cap(struct aos_rpc_session* sess,
        struct lmp_recv_msg* msg,
        struct capref received_capref,
        void* context,
        struct capref* ret_cap,
        uint32_t* ret_type,
        uint32_t* ret_flags)
{
    enum aos_rpc_cap_type requested_cap_type=(enum aos_rpc_cap_type)msg->words[1];

    DEBUG_LRPC("Received request for special capability\n");
    networking_lmp_chan=&sess->lc;

    if(requested_cap_type==AOS_CAP_IRQ){
    	DEBUG_LRPC("Sending aos irq table capability\n");
        ERROR_RET1(lmp_chan_send1(&sess->lc,
            LMP_FLAG_SYNC,
            cap_irq,
            MAKE_RPC_MSG_HEADER(RPC_SPECIAL_CAP_RESPONSE, RPC_FLAG_ACK)));
    }else if(requested_cap_type==AOS_CAP_NETWORK_UART){
    	DEBUG_LRPC("Sending uart frame capability\n");
        struct capref uart4_frame;
        slot_alloc(&uart4_frame);
        ERROR_RET1(frame_forge(uart4_frame, OMAP44XX_MAP_L4_PER_UART4, OMAP44XX_MAP_L4_PER_UART4_SIZE, 0));
        ERROR_RET1(lmp_chan_send1(&sess->lc,
            LMP_FLAG_SYNC,
            uart4_frame,
            MAKE_RPC_MSG_HEADER(RPC_SPECIAL_CAP_RESPONSE, RPC_FLAG_ACK)));
    }

    return SYS_ERR_OK;
}

static
errval_t handle_udp_connect(struct aos_rpc_session* sess,
        struct lmp_recv_msg* msg,
        struct capref received_capref,
        void* context,
        struct capref* ret_cap,
        uint32_t* ret_type,
        uint32_t* ret_flags)
{
	DEBUG_LRPC("---- Connecting to server!\n");

    if(networking_lmp_chan==NULL){
        return NETWORKING_ERR_NOT_AVAILABLE;
    }
    ERROR_RET1(wait_for_send(networking_lmp_chan, sess));
    ERROR_RET1(lmp_chan_send3(networking_lmp_chan,
        LMP_FLAG_SYNC,
        received_capref,
        UDP_PARSER_CONNECTION_CLIENT,
        msg->words[1],
        msg->words[2]));
    DEBUG_LRPC("Message sent to connect to server!\n");

    return SYS_ERR_OK;
}

static
errval_t handle_udp_create_server(struct aos_rpc_session* sess,
        struct lmp_recv_msg* msg,
        struct capref received_capref,
        void* context,
        struct capref* ret_cap,
        uint32_t* ret_type,
        uint32_t* ret_flags)
{
	DEBUG_LRPC("---- Creating server!\n");

    if(networking_lmp_chan==NULL){
        return NETWORKING_ERR_NOT_AVAILABLE;
    }

    ERROR_RET1(lmp_chan_send2(networking_lmp_chan,
        LMP_FLAG_SYNC,
        received_capref,
        UDP_PARSER_CONNECTION_SERVER,
        msg->words[1]));
    DEBUG_LRPC("Message sent to create server!\n");

    return SYS_ERR_OK;
}

static
errval_t handle_get_char_handle(struct aos_rpc_session* sess,
        struct lmp_recv_msg* msg,
        struct capref received_capref,
        void* context,
        struct capref* ret_cap,
        uint32_t* ret_type,
        uint32_t* ret_flags)
{
    char ret_char;
    sys_getchar(&ret_char);

    ERROR_RET1(lmp_chan_send2(&sess->lc,
        LMP_FLAG_SYNC,
        NULL_CAP,
        MAKE_RPC_MSG_HEADER(RPC_GET_CHAR, RPC_FLAG_ACK),
        ret_char));

    return SYS_ERR_OK;
}

static
errval_t handle_put_char_handle(struct aos_rpc_session* sess,
        struct lmp_recv_msg* msg,
        struct capref received_capref,
        void* context,
        struct capref* ret_cap,
        uint32_t* ret_type,
        uint32_t* ret_flags)
{
    sys_print((char*)(msg->words+1),1);
    return SYS_ERR_OK;
}

static
errval_t handle_set_led(struct aos_rpc_session* sess,
        struct lmp_recv_msg* msg,
        struct capref received_capref,
        void* context,
        struct capref* ret_cap,
        uint32_t* ret_type,
        uint32_t* ret_flags)
{
    int status = msg->words[1];
    DEBUG_LRPC("[handle_set_led] Set led status: %d\n", (int)status);

    struct capref frame_cap;
    ERROR_RET1(slot_alloc(&frame_cap));
    ERROR_RET1(frame_forge(frame_cap, 0x4A310000, BASE_PAGE_SIZE, my_core_id));
    void* buf;
    ERROR_RET1(paging_map_frame(get_current_paging_state(),
        (void*)&buf, BASE_PAGE_SIZE, frame_cap, NULL, NULL));
    int mask = 1 << 8;
    volatile int * out_enab = (volatile int*)(buf + 0x134);
    volatile int * dataout  = (volatile int*)(buf + 0x13C);
    *out_enab &= ~(mask);
    if (status == 0)
        *dataout &= ~mask;
    else if (status == 1)
        *dataout |= mask;
    return SYS_ERR_OK;
}

static
errval_t handle_memtest(struct aos_rpc_session* sess,
        struct lmp_recv_msg* msg,
        struct capref received_capref,
        void* context,
        struct capref* ret_cap,
        uint32_t* ret_type,
        uint32_t* ret_flags)
{
    lpaddr_t base = msg->words[1];
    size_t size = msg->words[2];
    DEBUG_LRPC("[handle_memtest] Mem tested [0x%08x - 0x%08x] OK\n",
        (int)base, (int)(base+size));
    struct capref frame_cap;
    ERROR_RET1(slot_alloc(&frame_cap));
    ERROR_RET1(frame_forge(frame_cap, base, size, my_core_id));
    char* buf;
    ERROR_RET1(paging_map_frame(get_current_paging_state(),
        (void*)&buf, size, frame_cap, NULL, NULL));
    memset(buf, 0, size);
    memset(buf, 0x42, size);
    return SYS_ERR_OK;
}

static
errval_t handle_bootstrap_nameserver(struct aos_rpc_session* sess,
        struct lmp_recv_msg* msg,
        struct capref received_capref,
        void* context,
        struct capref* ret_cap,
        uint32_t* ret_type,
        uint32_t* ret_flags)
{
    DEBUG_LRPC("Recv RPC_NAMESERVER_LOOKUP", 0);

    if (!ns_rpc.server_sess)
        return RPC_ERR_INVALID_ARGUMENTS;

    DEBUG_LRPC("Received bootstrapping request, forwarding to nameserver\n");

    // Forward lookup request to nameserver
    ERROR_RET1(wait_for_send(&ns_rpc.server_sess->lc, ns_rpc.server_sess));
    ERROR_RET1(lmp_chan_send1(&ns_rpc.server_sess->lc,
        LMP_FLAG_SYNC,
        NULL_CAP,
        RPC_NAMESERVER_EP_REQUEST));

    DEBUG_LRPC("Sent, waiting for nameserver response\n");

    // Relay cap back to client
    struct lmp_recv_msg message=LMP_RECV_MSG_INIT;
    struct capref relay_cap;
    ERROR_RET1(recv_block(ns_rpc.server_sess,
        &message,
        &relay_cap));

    DEBUG_LRPC("-------------------------------- Received and attempting to return the following endpoint:\n");
    struct capability cap;
    debug_cap_identify(relay_cap, &cap);
    DEBUG_LRPC("Cap type: 0x%x\n", cap.type);
    DEBUG_LRPC("\tListener: 0x%x\n", cap.u.endpoint.listener);
    DEBUG_LRPC("\tOffset: 0x%x\n", cap.u.endpoint.epoffset);

    ERROR_RET1(lmp_chan_send1(&sess->lc,
        LMP_FLAG_GIVEAWAY,
        relay_cap,
        MAKE_RPC_MSG_HEADER(RPC_NAMESERVER_LOOKUP, RPC_FLAG_ACK)));

    return SYS_ERR_OK;
}

errval_t lmp_server_init(struct aos_rpc* rpc)
{
    networking_lmp_chan=NULL;
    aos_rpc_register_handler(rpc, RPC_HANDSHAKE, handle_handshake, true);
    aos_rpc_register_handler(rpc, RPC_NAMESERVER_REGISTER, handle_nameserver_cap, true);
    aos_rpc_register_handler(rpc, RPC_NAMESERVER_LOOKUP, handle_bootstrap_nameserver, false);
    aos_rpc_register_handler(rpc, RPC_SHARED_BUFFER_REQUEST, handle_shared_buffer_request, true);
    aos_rpc_register_handler(rpc, RPC_NUMBER, handle_number, true);
    aos_rpc_register_handler(rpc, RPC_STRING, handle_string, true);
    aos_rpc_register_handler(rpc, RPC_RAM_CAP_QUERY, handle_ram_cap_opcode, false);
    aos_rpc_register_handler(rpc, RPC_GET_CHAR, handle_get_char_handle, false);
    aos_rpc_register_handler(rpc, RPC_PUT_CHAR, handle_put_char_handle, true);
    aos_rpc_register_handler(rpc, RPC_SPECIAL_CAP_QUERY, handle_get_special_cap, false);
    aos_rpc_register_handler(rpc, RPC_NETWORK_UDP_CONNECT, handle_udp_connect, true);
    aos_rpc_register_handler(rpc, RPC_NETWORK_UDP_CREATE_SERVER, handle_udp_create_server, true);
    aos_rpc_register_handler(rpc, RPC_SET_LED, handle_set_led, true);
    aos_rpc_register_handler(rpc, RPC_MEMTEST, handle_memtest, true);

    return SYS_ERR_OK;
}
