#ifndef _BINDING_SERVER_H_
#define _BINDING_SERVER_H_

#include <stdio.h>
#include <aos/aos.h>
#include <aos/aos_rpc.h>
#include <aos/urpc/urpc.h>
#include "urpc/handlers.h"
#include "urpc/opcodes.h"

struct open_connection{
    struct frame_identity frame_info;
    struct open_connection* next;
    uint32_t port;
};

struct binding_server_state{
    struct open_connection* head;
};

errval_t binding_server_lmp_init(struct aos_rpc* rpc, struct urpc_channel* channel);
errval_t binding_server_register_urpc_handlers(struct urpc_channel* channel);
//TODO: Add close socket call

#endif //_BINDING_SERVER_H_
