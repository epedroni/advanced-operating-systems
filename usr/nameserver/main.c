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

int main(int argc, char *argv[])
{
    debug_printf("Initialising RPC...\n");
    ERROR_RET1(aos_rpc_init(&nameserver_rpc, NULL_CAP, false, false));
    ERROR_RET1(lmp_server_init(&nameserver_rpc));

    debug_printf("Creating init session so we can communicate\n");
    // create session
    struct aos_rpc_session* sess = NULL;
    // initialise with null remote
    aos_server_add_client(&nameserver_rpc, &sess);
    // associate endpoint with session?
    aos_server_register_client(&nameserver_rpc, sess);

    debug_printf("Ack received: %x\n", sess->ack_received);
    debug_printf("Can send: %x\n", sess->can_send);
    debug_printf("LC Connstate: %x\n", sess->lc.connstate);
    debug_printf("LC Endpoint: %x\n", sess->lc.endpoint);

    debug_printf("Sending endpoint to init, it should now be able to shake hands with us\n");
    aos_rpc_send_nameserver_info(get_init_rpc(), cap_selfep);


    debug_printf("Entering accept loop forever\n");
    aos_rpc_accept(&nameserver_rpc);

    return SYS_ERR_OK;
}
