#ifndef _HEADER_INIT_PROCESSMGR
#define _HEADER_INIT_PROCESSMGR

#include <aos/aos.h>
#include <spawn/spawn.h>
#include <aos/aos_rpc.h>
#include "processmgr.h"

struct running_process{
    struct running_process *next, *prev;
    domainid_t pid;
    char *name;
    struct lmp_endpoint *endpoint;
};

struct core_processmgr_state{
    struct running_process *running_procs;
    coreid_t core_id;
    domainid_t next_pid;
    uint32_t running_count;
    struct processmgr_state* master_pm;
};

errval_t core_processmgr_init(struct core_processmgr_state* pm_state, coreid_t core_id, struct aos_rpc* rpc, const char* init_name);
errval_t core_processmgr_spawn_process(struct core_processmgr_state* pm_state, char* process_name, struct aos_rpc* rpc,
        coreid_t core_id, domainid_t *ret_pid);
void core_processmgr_register_rpc_handlers(struct aos_rpc* rpc);

#endif //_HEADER_INIT_PROCESSMGR
