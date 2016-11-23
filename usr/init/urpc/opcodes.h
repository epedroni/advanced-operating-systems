#ifndef _HEADER_URPC_OPCODES
#define _HEADER_URPC_OPCODES

struct urpc_message
{
    uint32_t opcode;
    uint32_t length;
    void* data;
};

enum urpc_opcodes
{
    URPC_OP_NULL = 0,
    URPC_OP_PRINT,
    URPC_OP_COUNT,
};

#endif
