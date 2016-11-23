#ifndef _HEADER_INIT_PROCESSMGR
#define _HEADER_INIT_PROCESSMGR

#include <aos/aos.h>
#include "urpc/server.h"

struct processmgr_process{
    struct processmgr_process *next, *prev;
    domainid_t pid;
    char *name;
    coreid_t core_id;
};

struct processmgr_state{
    struct processmgr_process *head;
    struct urpc_channel* urpc_channel;
    domainid_t next_pid;
    uint32_t running_count;
    coreid_t my_core_id;
};

errval_t processmgr_init(struct processmgr_state* pm_state, struct urpc_channel* urpc_channel, coreid_t my_coreid);
errval_t processmgr_register_process(struct processmgr_state* pm_state, const char* name, coreid_t core_id, domainid_t* new_pid);
errval_t processmgr_deregister_process(struct processmgr_state* pm_state, domainid_t pid);
errval_t processmgr_get_process_name(struct processmgr_state* pm_state, domainid_t pid, char** name, size_t buffer_len);
errval_t processmgr_get_proces_ids(struct processmgr_state* pm_state, const char* name, domainid_t* pids, size_t* number);

#if 0

struct running_process{
    struct running_process *next, *prev;
    domainid_t pid;
    char *name;
    struct lmp_endpoint *endpoint;
    coreid_t core_id;
};

struct processmgr_state{
    struct running_process *running_procs;
    uint32_t running_count;
    domainid_t next_pid;
};

//TODO: Implement these elementary process operations
errval_t processmgr_allocate_process_id(struct processmgr_state* pm_state, const char* process_name, coreid_t core_id,
        domainid_t* new_id);
errval_t processmgr_deallocate_process_id(struct processmgr_state* pm_state, int process_id);
errval_t processmgr_get_pid_by_name(struct processmgr_state* pm_state, const char* process_name, domainid_t* ids, size_t* count);
errval_t processmgr_get_name(struct processmgr_state* pm_state, domainid_t pid, char* process_name);
errval_t processmgr_spawn_process_with_id(struct processmgr_state* pm_state, const char* process_name, domainid_t* pid);

errval_t processmgr_init(struct processmgr_state* pm_state);
errval_t spawn_process(char* process_name, struct aos_rpc* rpc, coreid_t core_id, domainid_t *ret_pid);

//errval_t processmgr_init(struct aos_rpc* rpc, const char* init_name);
errval_t spawn_process(char* process_name, struct aos_rpc* rpc, coreid_t core_id, domainid_t *ret_pid);
void processmgr_register_rpc_handlers(struct aos_rpc* rpc);

#endif

#endif
