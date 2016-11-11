#ifndef _HEADER_INIT_PROCESSMGR
#define _HEADER_INIT_PROCESSMGR

errval_t processmgr_init(struct aos_rpc* rpc, const char* init_name);
errval_t spawn_process(char* process_name, struct aos_rpc* rpc, coreid_t core_id, domainid_t *ret_pid);
void processmgr_register_rpc_handlers(struct aos_rpc* rpc);

#endif
