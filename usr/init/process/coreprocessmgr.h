#ifndef _HEADER_INIT_COREPROCESSMGR
#define _HEADER_INIT_COREPROCESSMGR

#include <aos/aos.h>
#include <spawn/spawn.h>
#include <aos/aos_rpc.h>

struct running_process{
    struct running_process *next, *prev;
    domainid_t pid;
    struct lmp_endpoint *endpoint;
};

struct coreprocessmgr_state{
    struct running_process *running_procs;
    coreid_t core_id;
    struct processmgr_state* master_pm;
};

errval_t coreprocessmgr_init(struct coreprocessmgr_state* pm_state, coreid_t core_id, struct aos_rpc* rpc);
errval_t coreprocessmgr_spawn_process(struct coreprocessmgr_state* pm_state, const char* process_name, struct aos_rpc* rpc,
        coreid_t core_id, domainid_t withpid);
errval_t coreprocessmgr_find_process_by_endpoint(struct coreprocessmgr_state* pm_state, struct lmp_endpoint* ep, domainid_t* pid);
errval_t coreprocessmgr_process_finished(struct coreprocessmgr_state* pm_state, domainid_t pid);

#endif //_HEADER_INIT_PROCESSMGR
