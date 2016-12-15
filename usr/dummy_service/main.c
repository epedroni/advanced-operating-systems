/**
 * \file
 * \brief Dummy service application
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
#include <aos/nameserver.h>
#include "lrpc_server.h"

struct aos_rpc ns_rpc;
struct aos_rpc own_rpc;

int main(int argc, char *argv[])
{
    debug_printf("Initialising dummy RPC server...\n");
    ERROR_RET1(aos_rpc_init(&own_rpc, NULL_CAP, false, false));
    ERROR_RET1(lmp_server_init(&own_rpc));

    debug_printf("Binding with nameserver\n");
    nameserver_rpc_init(&ns_rpc);

    debug_printf("Registering service\n");
    struct aos_rpc_session* ns_sess = NULL;
    aos_server_add_client(&own_rpc, &ns_sess);
    aos_server_register_client(&own_rpc, ns_sess);
    nameserver_register(&ns_rpc, ns_sess->lc.local_cap, "dummy_service");

    // Handle requests
    debug_printf("Looping forever\n");
    aos_rpc_accept(&own_rpc);

    return SYS_ERR_OK;
}

