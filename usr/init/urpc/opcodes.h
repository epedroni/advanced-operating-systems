#ifndef _HEADER_URPC_OPCODES
#define _HEADER_URPC_OPCODES

enum urpc_opcodes
{
    URPC_OP_NULL = 0,
    URPC_OP_PRINT,
    URPC_OP_PROCESSMGR_GEN_PID,
    URPC_OP_PROCESSMGR_SPAWN,
    URPC_OP_GET_PROCESS_NAME,
    URPC_OP_GET_PROCESS_DEREGISTER,
    URPC_OP_LIST_PIDS,
    URPC_OP_CONNECT_TO_SOCKET,
    URPC_OP_COUNT,
};

// Message structures
struct urpc_msg_gen_pid
{
    size_t name_size;
    coreid_t core_id;
    char name[0];
};

struct urpc_msg_spawn
{
    size_t name_size;
    coreid_t core_id;
    domainid_t pid;
    char name[0];
};

struct urpc_msg_get_process_name
{
    domainid_t pid;
    size_t max_size;
};

#endif
