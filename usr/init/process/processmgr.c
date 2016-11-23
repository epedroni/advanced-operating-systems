#include "init.h"

#include "process/processmgr.h"
#include "process/sysprocessmgr.h"
#include "process/coreprocessmgr.h"

static struct sysprocessmgr_state syspmgr_state;
static struct coreprocessmgr_state core_pm_state;
static bool use_sysmgr;
static coreid_t my_core_id;

errval_t processmgr_init(coreid_t coreid)
{
    my_core_id = core_id;
    if (coreid == 0)
    {
        ERROR_RET1(sysprocessmgr_init(&syspmgr_state, &urpc_chan, my_core_id));
        use_sysmgr = true;
    }
    ERROR_RET1(coreprocessmgr_init(&core_pm_state, coreid, &rpc));
    processmgr_register_rpc_handlers(&rpc);
    return SYS_ERR_OK;
}

errval_t processmgr_generate_pid(const char* name, coreid_t core_id, domainid_t* new_pid)
{
    if (use_sysmgr)
        return sysprocessmgr_register_process(&syspmgr_state, name, core_id, new_pid);
    // URPC CALL TO GET PID
    return SYS_ERR_NOT_IMPLEMENTED;
}

errval_t processmgr_spawn_process(char* process_name, coreid_t core_id, domainid_t *pid)
{
    ERROR_RET1(processmgr_generate_pid(process_name, core_id, pid))
    if (core_id == my_core_id)
        return coreprocessmgr_spawn_process(&core_pm_state, process_name, &rpc, core_id, *pid);
    return SYS_ERR_NOT_IMPLEMENTED;
}

errval_t processmgr_get_process_name(domainid_t pid, char* name, size_t buffer_len)
{
    if (use_sysmgr)
        return sysprocessmgr_get_process_name(&syspmgr_state, pid, name, buffer_len);
    return SYS_ERR_NOT_IMPLEMENTED;
}

errval_t processmgr_list_pids(domainid_t* pids, size_t* number)
{
    if (use_sysmgr)
        return sysprocessmgr_list_pids(&syspmgr_state, pids, number);
    return SYS_ERR_NOT_IMPLEMENTED;
}


errval_t processmgr_process_exited(struct lmp_endpoint* ep)
{
    domainid_t pid;
    ERROR_RET1(coreprocessmgr_find_process_by_endpoint(&core_pm_state, ep, &pid));
    ERROR_RET1(coreprocessmgr_process_finished(&core_pm_state, ep));
    if (use_sysmgr)
        return sysprocessmgr_deregister_process(&syspmgr_state, pid);
    return SYS_ERR_NOT_IMPLEMENTED;
}
