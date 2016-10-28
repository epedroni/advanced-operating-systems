#include "lrpc_server.h"
#include <arch/arm/barrelfish_kpi/asm_inlines_arch.h>

#define DEBUG_LRPC(s, ...) debug_printf("[RPC] " s, ##__VA_ARGS__)

static const int SESSION_BUFF_SIZE=100;

static
errval_t handle_handshake(struct aos_rpc_session* sess, struct lmp_recv_msg* msg, struct capref received_capref,
        struct capref* ret_cap, uint32_t* ret_type, uint32_t* ret_flags)
{
    DEBUG_LRPC("Recv RPC_HANDSHAKE", 0);
    sess->lc.remote_cap=received_capref;

    sess->buffer=malloc(SESSION_BUFF_SIZE*sizeof(char));
    sess->buffer_capacity=SESSION_BUFF_SIZE;

    return SYS_ERR_OK;
}

static
errval_t handle_number(struct aos_rpc_session* sess, struct lmp_recv_msg* msg, struct capref received_capref,
        struct capref* ret_cap, uint32_t* ret_type, uint32_t* ret_flags)
{
    DEBUG_LRPC("Received number %d, it took %d cycles\n",msg->words[1], get_cycle_count());
    return SYS_ERR_OK;
}

static
errval_t handle_string(struct aos_rpc_session* sess, struct lmp_recv_msg* msg, struct capref received_capref,
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

static
errval_t handle_ram_cap_opcode(struct aos_rpc_session* sess,
        struct lmp_recv_msg* msg,
        struct capref received_capref,
        struct capref* ret_cap,
        uint32_t* ret_type,
        uint32_t* ret_flags)
{
    size_t requested_bytes = msg->words[1];
    size_t requested_aligment = msg->words[2];
    DEBUG_LRPC("Recvd RPC_RAM_CAP_QUERY [requested %d bytes | aligned 0x%x]\n",
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
        struct capref* ret_cap,
        uint32_t* ret_type,
        uint32_t* ret_flags)
{
    sys_print((char*)(msg->words+1),1);

//
//    ERROR_RET1(lmp_chan_send2(&sess->lc,
//        LMP_FLAG_SYNC,
//        NULL_CAP,
//        MAKE_RPC_MSG_HEADER(RPC_GET_CHAR, RPC_FLAG_ACK),
//        ret_char));
    return SYS_ERR_OK;
}

errval_t lmp_server_init(struct aos_rpc* rpc){

    aos_rpc_register_handler(rpc, RPC_HANDSHAKE, handle_handshake, true);
    aos_rpc_register_handler(rpc, RPC_NUMBER, handle_number, true);
    aos_rpc_register_handler(rpc, RPC_STRING, handle_string, true);
    aos_rpc_register_handler(rpc, RPC_RAM_CAP_QUERY, handle_ram_cap_opcode, false);
    aos_rpc_register_handler(rpc, RPC_GET_CHAR, handle_get_char_handle, false);
    aos_rpc_register_handler(rpc, RPC_PUT_CHAR, handle_put_char_handle, true);

    return SYS_ERR_OK;
}
