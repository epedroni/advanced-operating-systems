#ifndef _HEADER_INIT_PROCESSMGR
#define _HEADER_INIT_PROCESSMGR

#include <aos/aos.h>

struct lmp_endpoint;
struct urpc_channel;

errval_t processmgr_init(coreid_t core_id, const char* init_process_name);

errval_t processmgr_generate_pid(const char* name, coreid_t core_id, domainid_t* new_pid);
errval_t processmgr_spawn_process(char* process_name, coreid_t core_id, domainid_t *pid);
errval_t processmgr_spawn_process_with_pid(const char* process_name, coreid_t core_id, domainid_t pid);
errval_t processmgr_get_process_name(domainid_t pid, char* name, size_t buffer_len);
errval_t processmgr_list_pids(domainid_t* pids, size_t* number);
errval_t processmgr_process_exited(struct lmp_endpoint* ep);
errval_t processmgr_remove_pid(domainid_t pid);

void processmgr_register_rpc_handlers(struct aos_rpc* rpc);
errval_t processmgr_register_urpc_handlers(struct urpc_channel* channel);

#endif
