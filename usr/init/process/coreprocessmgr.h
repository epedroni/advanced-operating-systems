#ifndef _HEADER_INIT_COREPROCESSMGR
#define _HEADER_INIT_COREPROCESSMGR

#include <aos/aos.h>
#include <spawn/spawn.h>
#include <aos/aos_rpc.h>

struct running_process{
    struct running_process *next, *prev;
    domainid_t pid;
    char *name;
    struct lmp_endpoint *endpoint;
};

struct coreprocessmgr_state{
    struct running_process *running_procs;
    coreid_t core_id;
    domainid_t next_pid;
    uint32_t running_count;
    struct processmgr_state* master_pm;
};

errval_t coreprocessmgr_init(struct coreprocessmgr_state* pm_state, coreid_t core_id, struct aos_rpc* rpc, const char* init_name);
errval_t coreprocessmgr_spawn_process(struct coreprocessmgr_state* pm_state, char* process_name, struct aos_rpc* rpc,
        coreid_t core_id, domainid_t *ret_pid);
void coreprocessmgr_register_rpc_handlers(struct aos_rpc* rpc);

#endif //_HEADER_INIT_PROCESSMGR
