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
    debug_printf("Initialising RPC server...\n");
    ERROR_RET1(aos_rpc_init(&nameserver_rpc, NULL_CAP, false, false));
    ERROR_RET1(lmp_server_init(&nameserver_rpc));

    debug_printf("Creating new session for init\n");
    struct aos_rpc_session* init_sess = NULL;
    aos_server_add_client(&nameserver_rpc, &init_sess);
    aos_server_register_client(&nameserver_rpc, init_sess);

    debug_printf("Sending endpoint to init, it should now be able to shake hands with us\n");
    aos_rpc_nameserver_init(get_init_rpc(), init_sess->lc.local_cap);

    // Handle requests
    debug_printf("Looping forever\n");
    aos_rpc_accept(&nameserver_rpc);

    return SYS_ERR_OK;
}
