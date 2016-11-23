#include "init.h"

#include "process/processmgr.h"
#include "process/sysprocessmgr.h"
#include "process/coreprocessmgr.h"

static struct sysprocessmgr_state syspmgr_state;
static struct coreprocessmgr_state core_pm_state;
static bool use_sysmgr;

errval_t processmgr_init(coreid_t coreid, const char* init_process_name)
{
    if (coreid == 0)
    {
        ERROR_RET1(sysprocessmgr_init(&syspmgr_state, &urpc_chan, my_core_id));
        use_sysmgr = true;
    }
    ERROR_RET1(coreprocessmgr_init(&core_pm_state, coreid, &rpc));
    processmgr_register_rpc_handlers(&rpc);
    processmgr_register_urpc_handlers(&urpc_chan);

    domainid_t pid;
    ERROR_RET1(processmgr_generate_pid(init_process_name, coreid, &pid));

    return SYS_ERR_OK;
}

errval_t processmgr_generate_pid(const char* name, coreid_t core_id, domainid_t* new_pid)
{
    if (use_sysmgr)
        return sysprocessmgr_register_process(&syspmgr_state, name, core_id, new_pid);

    // URPC CALL: URPC_OP_PROCESSMGR_GEN_PID
    size_t size = strlen(name)+1;
    size_t send_size = size + sizeof(struct urpc_msg_gen_pid);
    struct urpc_msg_gen_pid* send = malloc(send_size);
    send->name_size = size;
    send->core_id = core_id;
    memcpy(send->name, name, size);
    domainid_t* answer_pid;
    size_t answer_len;

    ERROR_RET2(urpc_client_send(&urpc_chan.buffer_send, URPC_OP_PROCESSMGR_GEN_PID,
        send, send_size, (void**)&answer_pid, &answer_len),
        PROCMGR_ERR_URPC_REMOTE_FAIL);
    free(send);
    if (answer_len != sizeof(domainid_t))
    {
        free(answer_pid);
        return PROCMGR_ERR_URPC_REMOTE_FAIL;
    }

    *new_pid = (int)*answer_pid;
    free(answer_pid);

    return SYS_ERR_OK;
}

errval_t processmgr_spawn_process(char* name, coreid_t core_id, domainid_t *pid)
{
    debug_printf("processmgr_spawn_process:: Spawn on core %d\n", core_id);
    ERROR_RET1(processmgr_generate_pid(name, core_id, pid));
    debug_printf("processmgr_spawn_process:: Got PID %d\n", *pid);
    errval_t err;
    err=processmgr_spawn_process_with_pid(name, core_id, *pid);
    if(err_is_fail(err)){
        debug_printf("We have an error while spawning process, removing PID form list\n");
        ERROR_RET1(processmgr_remove_pid(*pid));
    }else{
        return err;
    }

    return SYS_ERR_OK;
}

errval_t processmgr_spawn_process_with_pid(const char* name, coreid_t core_id, domainid_t pid)
{
    if (core_id == my_core_id)
        return coreprocessmgr_spawn_process(&core_pm_state, name, &rpc, core_id, pid);

    // URPC CALL: URPC_OP_PROCESSMGR_SPAWN
    size_t size = strlen(name)+1;
    size_t send_size = size + sizeof(struct urpc_msg_spawn);
    struct urpc_msg_spawn* send = malloc(send_size);
    send->name_size = size;
    send->core_id = core_id;
    send->pid = pid;
    memcpy(send->name, name, size);
    errval_t* answer;
    size_t answer_len;

    errval_t err = urpc_client_send(&urpc_chan.buffer_send, URPC_OP_PROCESSMGR_SPAWN,
        send, send_size, (void**)&answer, &answer_len);

    if (err_is_fail(err))
        err = err_push(err, PROCMGR_ERR_URPC_REMOTE_FAIL);
    else
        err = *answer;

    free(send);
    free(answer);
    return err;
}

errval_t processmgr_get_process_name(domainid_t pid, char* name, size_t buffer_len)
{
    debug_printf("processmgr_get_process_name [pid = %d, buflen = %d]\n", pid, buffer_len);
    if (use_sysmgr)
        return sysprocessmgr_get_process_name(&syspmgr_state, pid, name, buffer_len);

    // URPC CALL: URPC_OP_GET_PROCESS_NAME
    struct urpc_msg_get_process_name query;
    query.pid = pid;
    query.max_size = buffer_len;
    ERROR_RET2(urpc_client_send_receive_fixed_size(&urpc_chan.buffer_send,
        URPC_OP_GET_PROCESS_NAME,
        (void*)&query, sizeof(query), name, buffer_len, NULL),
        PROCMGR_ERR_URPC_REMOTE_FAIL);

    return SYS_ERR_OK;
}

errval_t processmgr_list_pids(domainid_t* pids, size_t* number)
{
    if (use_sysmgr)
        return sysprocessmgr_list_pids(&syspmgr_state, pids, number);

    // URPC CALL: URPC_OP_LIST_PIDS
    size_t max_return_size = *number;
    ERROR_RET1(urpc_client_send_receive_fixed_size(&urpc_chan.buffer_send, URPC_OP_LIST_PIDS,
        (void*)&max_return_size, sizeof(max_return_size), pids, max_return_size, number));
    return SYS_ERR_OK;
}

errval_t processmgr_remove_pid(domainid_t pid){
    if(use_sysmgr)
        return sysprocessmgr_deregister_process(&syspmgr_state, pid);

    char* answer;
    size_t answer_len;
    return urpc_client_send(&urpc_chan.buffer_send,
            URPC_OP_GET_PROCESS_DEREGISTER,
            (void*)&pid,
            sizeof(pid),
            (void**)&answer,
            &answer_len);
}

errval_t processmgr_process_exited(struct lmp_endpoint* ep)
{
    domainid_t pid;
    ERROR_RET1(coreprocessmgr_find_process_by_endpoint(&core_pm_state, ep, &pid));
    ERROR_RET1(coreprocessmgr_process_finished(&core_pm_state, pid));

    debug_printf("Process exited: [PID: %d]\n", pid);

    if (use_sysmgr)
        return sysprocessmgr_deregister_process(&syspmgr_state, pid);

    debug_printf("Sending message to other core that process has finished!\n");

    return processmgr_remove_pid(pid);
}
