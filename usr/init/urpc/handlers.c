#include "urpc/urpc.h"
#include "urpc/handlers.h"

static errval_t urpc_handle_print_op(struct urpc_buffer* buf, struct urpc_message* msg)
{
    char* data_as_str = msg->data;
    data_as_str[msg->length] = 0;
    debug_printf("SERVER: urpc_handle_print_op: \"%s\"\n", data_as_str);

    urpc_server_answer(buf, "milan answered", sizeof("milan answered"));

    return SYS_ERR_OK;
}

void urpc_server_register_callbacks(urpc_callback_func_t* table)
{
    table[URPC_OP_PRINT] = urpc_handle_print_op;
}
