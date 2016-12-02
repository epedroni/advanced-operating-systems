#include "lrpc_server.h"
#include "init.h"
#include <arch/arm/barrelfish_kpi/asm_inlines_arch.h>
#include <aos/paging.h>

#define DEBUG_LRPC(s, ...) //debug_printf("[RPC] " s "\n", ##__VA_ARGS__)

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

    debug_printf("Recv RPC_STRING [string size %d]\n", string_size);
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
    debug_printf("[handle_set_led] Set led status: %d\n", (int)status);
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
    debug_printf("[handle_memtest] Mem tested [0x%08x - 0x%08x] OK\n",
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

errval_t lmp_server_init(struct aos_rpc* rpc)
{
    aos_rpc_register_handler(rpc, RPC_HANDSHAKE, handle_handshake, true);
    aos_rpc_register_handler(rpc, RPC_SHARED_BUFFER_REQUEST, handle_shared_buffer_request, true);
    aos_rpc_register_handler(rpc, RPC_NUMBER, handle_number, true);
    aos_rpc_register_handler(rpc, RPC_STRING, handle_string, true);
    aos_rpc_register_handler(rpc, RPC_RAM_CAP_QUERY, handle_ram_cap_opcode, false);
    aos_rpc_register_handler(rpc, RPC_GET_CHAR, handle_get_char_handle, false);
    aos_rpc_register_handler(rpc, RPC_PUT_CHAR, handle_put_char_handle, true);
    aos_rpc_register_handler(rpc, RPC_SET_LED, handle_set_led, true);
    aos_rpc_register_handler(rpc, RPC_MEMTEST, handle_memtest, true);

    return SYS_ERR_OK;
}
