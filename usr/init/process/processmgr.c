#include <aos/serializers.h>
#include "init.h"
#include "process/processmgr.h"
#include "process/sysprocessmgr.h"
#include "process/coreprocessmgr.h"

static struct sysprocessmgr_state syspmgr_state;
static struct coreprocessmgr_state core_pm_state;
static bool use_sysmgr;

#define PMGR_DEBUG(...) //debug_printf(__VA_ARGS__);

errval_t processmgr_init(coreid_t coreid, const char* init_process_name)
{
    if (coreid == 0)
    {
        ERROR_RET1(sysprocessmgr_init(&syspmgr_state, &urpc_chan, my_core_id));
        use_sysmgr = true;
    }
    ERROR_RET1(coreprocessmgr_init(&core_pm_state, coreid, &core_rpc));
    processmgr_register_rpc_handlers(&core_rpc);
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
    char* argv[1] = {name};
    return processmgr_spawn_process_with_args(argv, 1, core_id, pid);
}

errval_t processmgr_spawn_process_with_args(char* const argv[], int argc, coreid_t core_id, domainid_t *pid)
{
    assert(argc > 0 && "Spawning process with 0 arg?! Need at least process name in argv[0]");

    debug_printf("[ProcessMgr] Spawn on core %d\n", core_id);
    ERROR_RET1(processmgr_generate_pid(argv[0], core_id, pid));
    debug_printf("[ProcessMgr] Generated PID %d\n", *pid);
    errval_t err;
    err=processmgr_spawn_process_with_args_and_pid(argv, argc, core_id, *pid);
    if(err_is_fail(err)){
        debug_printf("[ProcessMgr] We have an error while spawning process, removing PID form list\n");
        processmgr_remove_pid(*pid);
        return err;
    }
    return SYS_ERR_OK;
}

errval_t processmgr_spawn_process_with_args_and_pid(char* const argv[], int argc, coreid_t core_id, domainid_t pid)
{
    if (core_id == my_core_id)
        return coreprocessmgr_spawn_process(&core_pm_state, argv, argc, &core_rpc, core_id, pid);

    // URPC CALL: URPC_OP_PROCESSMGR_SPAWN
    size_t size = serialize_array_of_strings_size(argv, argc);
    size_t send_size = size + sizeof(coreid_t) + sizeof(domainid_t);
    void* send = malloc(send_size);
    memcpy(send,                    &core_id,   sizeof(coreid_t));
    memcpy(send + sizeof(coreid_t), &pid,       sizeof(domainid_t));
    if (!serialize_array_of_strings(send + sizeof(coreid_t) + sizeof(domainid_t),
            size, argv, argc))
        return AOS_ERR_SERIALIZE;

    errval_t* answer;
    size_t answer_len = 0;

    errval_t err = urpc_client_send(&urpc_chan.buffer_send, URPC_OP_PROCESSMGR_SPAWN,
        send, send_size, (void**)&answer, &answer_len);

    if (err_is_fail(err))
        err = err_push(err, PROCMGR_ERR_URPC_REMOTE_FAIL);
    else
        err = *answer;

    free(send);
    if (answer_len)
        free(answer);
    return err;
}

errval_t processmgr_get_process_name(domainid_t pid, char* name, size_t buffer_len)
{
    PMGR_DEBUG("processmgr_get_process_name [pid = %d, buflen = %d]\n", pid, buffer_len);
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
    PMGR_DEBUG("processmgr_list_pids: Maximum size %d\n", *number);
    if (use_sysmgr)
        return sysprocessmgr_list_pids(&syspmgr_state, pids, number);

    // URPC CALL: URPC_OP_LIST_PIDS
    size_t max_return_size = *number;
    ERROR_RET1(urpc_client_send_receive_fixed_size(&urpc_chan.buffer_send, URPC_OP_LIST_PIDS,
        (void*)&max_return_size, sizeof(max_return_size), pids, max_return_size, number));
    *number = *number / sizeof(domainid_t);
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

    debug_printf("[ProcessMgr] Process exited: [PID: %d]\n", pid);

    if (use_sysmgr)
        return sysprocessmgr_deregister_process(&syspmgr_state, pid);

    return processmgr_remove_pid(pid);
}
