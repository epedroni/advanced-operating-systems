#ifndef _HEADER_URPC_OPCODES
#define _HEADER_URPC_OPCODES

struct urpc_message
{
    uint32_t opcode;
    uint32_t length; // Length of $data
    void* data;
};

enum urpc_opcodes
{
    URPC_OP_NULL = 0,
    URPC_OP_PRINT,
    URPC_OP_PROCESSMGR_GEN_PID,
    URPC_OP_PROCESSMGR_SPAWN_REMOTE,
    URPC_OP_COUNT,
};

// Message structures
struct urpc_msg_gen_pid
{
    size_t name_size;
    coreid_t core_id;
    char name[0];
};

#endif
