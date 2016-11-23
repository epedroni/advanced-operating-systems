#include "urpc/urpc.h"
#include "urpc/handlers.h"

static errval_t urpc_handle_print_op(struct urpc_buffer* buf, struct urpc_message* msg, void* context)
{
    char* data_as_str = msg->data;
    data_as_str[msg->length] = 0;
    debug_printf("SERVER: urpc_handle_print_op: \"%s\"\n", data_as_str);

    urpc_server_answer(buf, "answer", sizeof("answer"));

    return SYS_ERR_OK;
}

errval_t urpc_register_default_handlers(struct urpc_channel* channel){
    urpc_server_register_handler(channel, URPC_OP_PRINT, urpc_handle_print_op, NULL);

    return SYS_ERR_OK;
}
