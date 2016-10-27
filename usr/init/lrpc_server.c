#include "lrpc_server.h"

static
errval_t handle_handshake(void* context, struct aos_rpc_session* sess, struct lmp_recv_msg* msg, struct capref received_capref,
        struct capref* ret_cap, uint32_t* ret_type, uint32_t* ret_flags)
{
    sess->lc.remote_cap=received_capref;
    return SYS_ERR_OK;
}

static
errval_t handle_number(void* context, struct aos_rpc_session* sess, struct lmp_recv_msg* msg, struct capref received_capref,
        struct capref* ret_cap, uint32_t* ret_type, uint32_t* ret_flags)
{
    debug_printf("Received number %d\n",msg->words[1]);
    return SYS_ERR_OK;
}

static
errval_t handle_string(void* context, struct aos_rpc_session* sess, struct lmp_recv_msg* msg, struct capref received_capref,
        struct capref* ret_cap, uint32_t* ret_type, uint32_t* ret_flags){

    if(getMessageFlags(msg) & RPC_FLAG_INCOMPLETE){


        debug_printf("Received only part of string\n");
    }else{
        debug_printf("Received end of string\n");
        debug_printf("Received string %s\n", msg->words+1);
    }

    return SYS_ERR_OK;
}

static
errval_t handle_ram_cap_opcode(void* context,
        struct aos_rpc_session* sess,
        struct lmp_recv_msg* msg,
        struct capref received_capref,
        struct capref* ret_cap,
        uint32_t* ret_type,
        uint32_t* ret_flags)
{
    size_t requested_bytes = msg->words[1];
    debug_printf("Recvd RPC_RAM_CAP_QUERY [requested %d bytes]\n", (int)requested_bytes);

    struct capref return_cap;
    ERROR_RET1(ram_alloc(&return_cap, requested_bytes));
    ERROR_RET1(lmp_chan_send2(&sess->lc,
        LMP_FLAG_SYNC,
        return_cap,
        MAKE_RPC_MSG_HEADER(RPC_RAM_CAP_RESPONSE, RPC_FLAG_ACK),
        requested_bytes));
    return SYS_ERR_OK;
}

errval_t lmp_server_init(struct aos_rpc* rpc, struct lmp_server_state* lmp_state)
{
    lmp_state->current_buff_position=0;

    aos_rpc_register_handler(rpc, RPC_HANDSHAKE, handle_handshake, true, lmp_state);
    aos_rpc_register_handler(rpc, RPC_NUMBER, handle_number, true, lmp_state);
    aos_rpc_register_handler(rpc, RPC_STRING, handle_string, true, lmp_state);
    aos_rpc_register_handler(rpc, RPC_RAM_CAP_QUERY, handle_ram_cap_opcode, false);

    return SYS_ERR_OK;
}
