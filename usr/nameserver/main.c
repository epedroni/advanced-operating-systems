/**
 * \file
 * \brief Name server application
 */

/*
 * Copyright (c) 2016 ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, CAB F.78, Universitaetstr. 6, CH-8092 Zurich,
 * Attn: Systems Group.
 */

#include <stdio.h>
#include <aos/aos.h>
#include <aos/aos_rpc.h>
#include "lrpc_server.h"

struct aos_rpc nameserver_rpc;

static
errval_t handle_nameserver_lookup(struct aos_rpc_session* sess,
        struct lmp_recv_msg* msg,
        struct capref received_capref,
        void* context,
        struct capref* ret_cap,
        uint32_t* ret_type,
        uint32_t* ret_flags)
{
    // need to look at the message, but not for now
    debug_printf("Received lookup request, creating new endpoint\n");

    // create a new session to bind with the client
    struct aos_rpc_session* init_sess = NULL;
    aos_server_add_client(&nameserver_rpc, &init_sess);
    aos_server_register_client(&nameserver_rpc, init_sess);

    debug_printf("Return endpoint to client\n");

    debug_printf("---------------------------------------------- Attempting to return the following endpoint:\n");
    struct capability cap;
    debug_cap_identify(init_sess->lc.local_cap, &cap);
    debug_printf("Cap type: 0x%x\n", cap.type);
    debug_printf("\tListener: 0x%x\n", cap.u.endpoint.listener);
    debug_printf("\tOffset: 0x%x\n", cap.u.endpoint.epoffset);

    ERROR_RET1(lmp_chan_send1(&sess->lc,
        LMP_FLAG_SYNC,
        init_sess->lc.local_cap,
        MAKE_RPC_MSG_HEADER(RPC_NAMESERVER_LOOKUP, RPC_FLAG_ACK)));

    return SYS_ERR_OK;
}

int main(int argc, char *argv[])
{
    debug_printf("Initialising RPC...\n");
    ERROR_RET1(aos_rpc_init(&nameserver_rpc, NULL_CAP, false, false));
    ERROR_RET1(lmp_server_init(&nameserver_rpc));
    aos_rpc_register_handler(&nameserver_rpc, RPC_NAMESERVER_LOOKUP, handle_nameserver_lookup, false);

    debug_printf("Registering init as a client\n");
    struct aos_rpc_session* init_sess = NULL;
    aos_server_add_client(&nameserver_rpc, &init_sess);
    aos_server_register_client(&nameserver_rpc, init_sess);

    debug_printf("Sending endpoint to init, it should now be able to shake hands with us\n");
    aos_rpc_send_nameserver_info(get_init_rpc(), init_sess->lc.local_cap);

    // Handle requests
    debug_printf("Looping forever\n");
    aos_rpc_accept(&nameserver_rpc);

    return SYS_ERR_OK;
}

