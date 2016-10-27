#include "lrpc_server.h"

//typedef errval_t (*aos_rpc_handler)(void* context, struct lmp_chan* lc, struct lmp_recv_msg* msg, struct capref received_capref,
//        struct capref* ret_cap, uint32_t* ret_type);

static
errval_t handle_handshake(void* context, struct lmp_chan* lc, struct lmp_recv_msg* msg, struct capref received_capref,
        struct capref* ret_cap, uint32_t* ret_type, uint32_t* ret_flags){

    debug_printf("Received handshake message!\n");
    lc->remote_cap=received_capref;

    return SYS_ERR_OK;
}

static
errval_t handle_number(void* context, struct lmp_chan* lc, struct lmp_recv_msg* msg, struct capref received_capref,
        struct capref* ret_cap, uint32_t* ret_type, uint32_t* ret_flags){

    debug_printf("Received number handshake %d\n",msg->words[1]);

    return SYS_ERR_OK;
}

static
errval_t handle_string(void* context, struct lmp_chan* lc, struct lmp_recv_msg* msg, struct capref received_capref,
        struct capref* ret_cap, uint32_t* ret_type, uint32_t* ret_flags){

    if(getMessageFlags(msg) & RPC_FLAG_INCOMPLETE){


        debug_printf("Received only part of string\n");
    }else{
        debug_printf("Received end of string\n");
        debug_printf("Received string %s\n", msg->words+1);
    }

    return SYS_ERR_OK;
}

errval_t lmp_server_init(struct aos_rpc* rpc, struct lmp_server_state* lmp_state){

    lmp_state->current_buff_position=0;

    aos_rpc_register_handler(rpc, RPC_HANDSHAKE, handle_handshake, true, lmp_state);
    aos_rpc_register_handler(rpc, RPC_NUMBER, handle_number, true, lmp_state);
    aos_rpc_register_handler(rpc, RPC_STRING, handle_string, true, lmp_state);

    return SYS_ERR_OK;
}

errval_t aos_server_add_client(struct aos_rpc* rpc, struct lmp_chan** chan);
{
    *chan = malloc(sizeof(struct lmp_chan));
    ERROR_RET1(lmp_chan_accept(*chan,
            DEFAULT_LMP_BUF_WORDS,
            NULL_CAP));
    ERROR_RET1(lmp_chan_alloc_recv_slot(*chan));
    return SYS_ERR_OK;
}

errval_t aos_server_register_client(struct aos_rpc* rpc, struct lmp_chan* chan);
{
    lmp_chan_register_recv(chan, rpc->ws, MKCLOSURE(cb_accept_loop, rpc));
    return SYS_ERR_OK;
}
