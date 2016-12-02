#ifndef _HEADER_INIT_URPC_HANDLERS
#define _HEADER_INIT_URPC_HANDLERS

#include <aos/urpc/server.h>
#include "opcodes.h"
#include <aos/urpc/urpc.h>

#define URPC_PROTOCOL_ASSERT(cond) { if (!(cond)) return URPC_ERR_PROTOCOL_ERROR; }
#define URPC_CHECK_READ_SIZE(msg, size) {if ((msg)->length < size) return URPC_ERR_PROTOCOL_ERROR; (msg)->length -= size; }

errval_t urpc_register_default_handlers(struct urpc_channel* channel);

#endif
