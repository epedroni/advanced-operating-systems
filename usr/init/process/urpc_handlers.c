#include "init.h"
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

static errval_t urpc_handle_spawn(struct urpc_buffer* buf, struct urpc_message* msg, void* context)
{
    URPC_CHECK_READ_SIZE(msg, sizeof(struct urpc_msg_spawn));
    struct urpc_msg_spawn* data = msg->data;
    URPC_CHECK_READ_SIZE(msg, data->name_size);
    data->name[data->name_size - 1] = 0;

    if (data->core_id != my_core_id)
        return PROCMGR_ERR_REMOTE_DIFFERENT_COREID;

    ERROR_RET1(processmgr_spawn_process_with_pid(data->name, data->core_id, data->pid));
    return SYS_ERR_OK;
}

static errval_t urpc_handle_get_name(struct urpc_buffer* buf, struct urpc_message* msg, void* context)
{
    URPC_CHECK_READ_SIZE(msg, sizeof(struct urpc_msg_get_process_name));
    struct urpc_msg_get_process_name* query = msg->data;
    char* name = malloc(query->max_size);

    errval_t err = processmgr_get_process_name(query->pid, name, query->max_size);
    if (err_is_fail(err))
    {
        free(name);
        return err;
    }
    ERROR_RET1(urpc_server_answer(buf, name, query->max_size));
    free(name);
    return SYS_ERR_OK;
}

static errval_t urpc_handle_list_pids(struct urpc_buffer* buf, struct urpc_message* msg, void* context)
{
    URPC_CHECK_READ_SIZE(msg, sizeof(size_t));
    size_t max_size = *((size_t*)msg->data);
    domainid_t* pids = malloc(max_size);

    errval_t err = processmgr_list_pids(pids, &max_size);
    if (err_is_fail(err))
    {
        free(pids);
        return err;
    }
    ERROR_RET1(urpc_server_answer(buf, pids, max_size));
    free(pids);
    return SYS_ERR_OK;
}

errval_t processmgr_register_urpc_handlers(struct urpc_channel* channel)
{
    urpc_server_register_handler(channel, URPC_OP_PROCESSMGR_GEN_PID, urpc_handle_gen_pid, NULL);
    urpc_server_register_handler(channel, URPC_OP_PROCESSMGR_SPAWN, urpc_handle_spawn, NULL);
    urpc_server_register_handler(channel, URPC_OP_GET_PROCESS_NAME, urpc_handle_get_name, NULL);
    urpc_server_register_handler(channel, URPC_OP_LIST_PIDS, urpc_handle_list_pids, NULL);
    return SYS_ERR_OK;
}
