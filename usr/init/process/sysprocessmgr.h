#ifndef _HEADER_INIT_SYSPROCESSMGR
#define _HEADER_INIT_SYSPROCESSMGR

#include <aos/aos.h>
#include "urpc/server.h"

struct sysprocessmgr_process{
    struct sysprocessmgr_process *next, *prev;
    domainid_t pid;
    char *name;
    coreid_t core_id;
};

struct sysprocessmgr_state{
    struct sysprocessmgr_process *head;
    struct urpc_channel* urpc_channel;
    domainid_t next_pid;
    uint32_t running_count;
    coreid_t my_core_id;
};

errval_t sysprocessmgr_init(struct sysprocessmgr_state* pm_state, struct urpc_channel* urpc_channel, coreid_t my_coreid);
errval_t sysprocessmgr_register_process(struct sysprocessmgr_state* pm_state, const char* name, coreid_t core_id, domainid_t* new_pid);
errval_t sysprocessmgr_deregister_process(struct sysprocessmgr_state* pm_state, domainid_t pid);
errval_t sysprocessmgr_get_process_name(struct sysprocessmgr_state* pm_state, domainid_t pid, char* name, size_t buffer_len);
errval_t sysprocessmgr_list_pids(struct sysprocessmgr_state* pm_state, domainid_t* pids, size_t* number);

#endif
