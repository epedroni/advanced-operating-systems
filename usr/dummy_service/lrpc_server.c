#include "lrpc_server.h"

#include <omap44xx_map.h>

#define DEBUG_LRPC(s, ...) debug_printf("[RPC] " s "\n", ##__VA_ARGS__)

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
errval_t handle_nameserver_lookup(struct aos_rpc_session* sess,
        struct lmp_recv_msg* msg,
        struct capref received_capref,
        void* context,
        struct capref* ret_cap,
        uint32_t* ret_type,
        uint32_t* ret_flags)
{
    // need to look at the message, but not for now
    debug_printf("Received lookup request, creating new endpoint\n");

    // create a new session to bind with the client
    struct aos_rpc_session* init_sess = NULL;
    aos_server_add_client(sess->rpc, &init_sess);
    aos_server_register_client(sess->rpc, init_sess);

    debug_printf("Return endpoint to client\n");

    debug_printf("---------------------------------------------- Attempting to return the following endpoint:\n");
    struct capability cap;
    debug_cap_identify(init_sess->lc.local_cap, &cap);
    debug_printf("Cap type: 0x%x\n", cap.type);
    debug_printf("\tListener: 0x%x\n", cap.u.endpoint.listener);
    debug_printf("\tOffset: 0x%x\n", cap.u.endpoint.epoffset);

    ERROR_RET1(lmp_chan_send1(&sess->lc,
        LMP_FLAG_SYNC,
        init_sess->lc.local_cap,
        MAKE_RPC_MSG_HEADER(RPC_NAMESERVER_LOOKUP, RPC_FLAG_ACK)));

    return SYS_ERR_OK;
}

errval_t lmp_server_init(struct aos_rpc* rpc)
{
    aos_rpc_register_handler(rpc, RPC_HANDSHAKE, handle_handshake, true);
    aos_rpc_register_handler(rpc, RPC_SHARED_BUFFER_REQUEST, handle_shared_buffer_request, true);
    aos_rpc_register_handler(rpc, RPC_STRING, handle_string, true);
    aos_rpc_register_handler(rpc, RPC_NAMESERVER_LOOKUP, handle_nameserver_lookup, false);

    return SYS_ERR_OK;
}
