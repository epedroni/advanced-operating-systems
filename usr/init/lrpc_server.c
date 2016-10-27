#include "lrpc_server.h"

//typedef errval_t (*aos_rpc_handler)(void* context, struct lmp_chan* lc, struct lmp_recv_msg* msg, struct capref received_capref,
//        struct capref* ret_cap, uint32_t* ret_type);

static const int SESSION_BUFF_SIZE=100;

static
errval_t handle_handshake(void* context, struct aos_rpc_session* sess, struct lmp_recv_msg* msg, struct capref received_capref,
        struct capref* ret_cap, uint32_t* ret_type, uint32_t* ret_flags){

    debug_printf("Received handshake message!\n");
    sess->lc.remote_cap=received_capref;

    sess->buffer=malloc(SESSION_BUFF_SIZE*sizeof(char));
    sess->buffer_capacity=SESSION_BUFF_SIZE;

    return SYS_ERR_OK;
}

static
errval_t handle_number(void* context, struct aos_rpc_session* sess, struct lmp_recv_msg* msg, struct capref received_capref,
        struct capref* ret_cap, uint32_t* ret_type, uint32_t* ret_flags){

    debug_printf("Received number handshake %d\n",msg->words[1]);
    return SYS_ERR_OK;
}

static
errval_t handle_string(void* context, struct aos_rpc_session* sess, struct lmp_recv_msg* msg, struct capref received_capref,
        struct capref* ret_cap, uint32_t* ret_type, uint32_t* ret_flags){

    if(get_message_flags(msg) & RPC_FLAG_INCOMPLETE){

        if(sess->current_buff_position+LMP_MAX_BUFF_SIZE<sess->buffer_capacity){
            memcpy(sess->buffer+sess->current_buff_position, ((char*)msg->words)+sizeof(uintptr_t), LMP_MAX_BUFF_SIZE);
            sess->current_buff_position+=LMP_MAX_BUFF_SIZE;
        }else{
            return LIB_ERR_LMP_RECV_BUF_OVERFLOW;
        }

    }else{
        strncpy(sess->buffer+sess->current_buff_position, ((char*)msg->words)+sizeof(uintptr_t), LMP_MAX_BUFF_SIZE);
        sess->current_buff_position=0;

        sys_print(sess->buffer, strlen(sess->buffer)+1);
    }

    return SYS_ERR_OK;
}

errval_t lmp_server_init(struct aos_rpc* rpc){

    aos_rpc_register_handler(rpc, RPC_HANDSHAKE, handle_handshake, true, NULL);
    aos_rpc_register_handler(rpc, RPC_NUMBER, handle_number, true, NULL);
    aos_rpc_register_handler(rpc, RPC_STRING, handle_string, true, NULL);

    return SYS_ERR_OK;
}
