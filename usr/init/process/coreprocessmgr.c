#include "process/coreprocessmgr.h"
#include "process/processmgr.h"

// ProcessMgr functions
errval_t coreprocessmgr_spawn_process(struct coreprocessmgr_state* pm_state, char* process_name,
        struct aos_rpc* rpc, coreid_t core_id, domainid_t withpid){
    errval_t err;

    //TODO: Ask main PM for new pid for process

    struct aos_rpc_session* sess = NULL;
    ERROR_RET1(aos_server_add_client(rpc, &sess));

    struct spawninfo* process_info = malloc(sizeof(struct spawninfo));
    process_info->core_id=core_id;
    err = spawn_load_by_name(process_name,
        process_info,
        &sess->lc);
    free(process_info);
    if (err_is_fail(err))
        return err;

    ERROR_RET1(aos_server_register_client(rpc, sess));

    // add to running processes list
    struct running_process *rp = malloc(sizeof(struct running_process));
    rp->prev = NULL;
    rp->next = pm_state->running_procs;
    if (rp->next) {
        rp->next->prev = rp;
    }
    rp->pid = withpid;
    rp->endpoint = sess->lc.endpoint;

    debug_printf("Spawned process with endpoint 0x%x\n", rp->endpoint);

    pm_state->running_procs = rp;
    return SYS_ERR_OK;
}


errval_t coreprocessmgr_init(struct coreprocessmgr_state* pm_state, coreid_t core_id,
        struct aos_rpc* rpc)
{
    pm_state->core_id = core_id;

    struct running_process *init_rp = malloc(sizeof(struct running_process));
    init_rp->prev = NULL;
    init_rp->next = NULL;
    init_rp->pid = core_id;

    init_rp->endpoint = NULL;
    pm_state->running_procs=init_rp;
    processmgr_register_rpc_handlers(rpc);
    return SYS_ERR_OK;
}


errval_t coreprocessmgr_find_process_by_endpoint(struct coreprocessmgr_state* pm_state, struct lmp_endpoint* ep, domainid_t* pid)
{
    struct running_process *rp = pm_state->running_procs;

    while (rp)
    {
        if (rp->endpoint == ep)
        {
            *pid = rp->pid;
            return SYS_ERR_OK;
        }
        rp = rp->next;
    }
    return PROCMGR_ERR_PROCESS_NOT_FOUND;
}

errval_t coreprocessmgr_process_finished(struct coreprocessmgr_state* pm_state, domainid_t pid)
{
    struct running_process *rp = pm_state->running_procs;
    while (rp && rp->pid != pid)
        rp = rp->next;
    if (!rp)
        return PROCMGR_ERR_PROCESS_NOT_FOUND;

    if (rp->next)
        rp->next->prev = rp->prev;
    if (rp->prev)
        rp->prev->next = rp->next;
    if (rp == pm_state->running_procs)
        pm_state->running_procs = rp->next;

    free(rp);
    return SYS_ERR_OK;
}
