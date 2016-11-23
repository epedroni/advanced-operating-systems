#include "urpc/urpc.h"
#include "urpc/handlers.h"
#include "process/processmgr.h"

static errval_t urpc_handle_gen_pid(struct urpc_buffer* buf, struct urpc_message* msg, void* context)
{
    URPC_CHECK_READ_SIZE(msg, sizeof(struct urpc_msg_gen_pid));
    struct urpc_msg_gen_pid* data = msg->data;
    URPC_CHECK_READ_SIZE(msg, data->name_size);
    data->name[data->name_size - 1] = 0;

    domainid_t pid;
    ERROR_RET1(processmgr_generate_pid(data->name, data->core_id, &pid));

    ERROR_RET1(urpc_server_answer(buf, &pid, sizeof(pid)));
    return SYS_ERR_OK;
}

errval_t processmgr_register_urpc_handlers(struct urpc_channel* channel)
{
    urpc_server_register_handler(channel, URPC_OP_PROCESSMGR_GEN_PID, urpc_handle_gen_pid, NULL);
    return SYS_ERR_OK;
}
