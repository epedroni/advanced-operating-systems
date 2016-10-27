#include "lrpc_server.h"


static const int SESSION_BUFF_SIZE=100;

static
errval_t handle_handshake(struct aos_rpc_session* sess, struct lmp_recv_msg* msg, struct capref received_capref,
        struct capref* ret_cap, uint32_t* ret_type, uint32_t* ret_flags)
{
    sess->lc.remote_cap=received_capref;

    sess->buffer=malloc(SESSION_BUFF_SIZE*sizeof(char));
    sess->buffer_capacity=SESSION_BUFF_SIZE;

    return SYS_ERR_OK;
}

static
errval_t handle_number(struct aos_rpc_session* sess, struct lmp_recv_msg* msg, struct capref received_capref,
        struct capref* ret_cap, uint32_t* ret_type, uint32_t* ret_flags)
{
    debug_printf("Received number %d\n",msg->words[1]);
    return SYS_ERR_OK;
}

static
errval_t handle_string(struct aos_rpc_session* sess, struct lmp_recv_msg* msg, struct capref received_capref,
        struct capref* ret_cap, uint32_t* ret_type, uint32_t* ret_flags){

    if(get_message_flags(msg) & RPC_FLAG_INCOMPLETE){

        debug_printf("Received only part of string, starting from %d\n", sess->current_buff_position);

        if(sess->current_buff_position+LMP_MAX_BUFF_SIZE<sess->buffer_capacity){
            memcpy(sess->buffer+sess->current_buff_position, ((char*)msg->words)+sizeof(uintptr_t), LMP_MAX_BUFF_SIZE);
            sess->current_buff_position+=LMP_MAX_BUFF_SIZE;
        }else{
            return LIB_ERR_LMP_RECV_BUF_OVERFLOW;
        }

    }else{
        strncpy(sess->buffer+sess->current_buff_position, ((char*)msg->words)+sizeof(uintptr_t), LMP_MAX_BUFF_SIZE);
        sess->current_buff_position=0;

        debug_printf("Received string %s\n", sess->buffer);
    }

    return SYS_ERR_OK;
}

static
errval_t handle_ram_cap_opcode(struct aos_rpc_session* sess,
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

errval_t lmp_server_init(struct aos_rpc* rpc){

    aos_rpc_register_handler(rpc, RPC_HANDSHAKE, handle_handshake, true);
    aos_rpc_register_handler(rpc, RPC_NUMBER, handle_number, true);
    aos_rpc_register_handler(rpc, RPC_STRING, handle_string, true);
    aos_rpc_register_handler(rpc, RPC_RAM_CAP_QUERY, handle_ram_cap_opcode, false);

    return SYS_ERR_OK;
}
