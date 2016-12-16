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

struct aos_rpc own_rpc;

int main(int argc, char *argv[])
{
    debug_printf("Initialising dummy RPC server...\n");
    ERROR_RET1(aos_rpc_init(&own_rpc, NULL_CAP, false));
    ERROR_RET1(lmp_server_init(&own_rpc));

    size_t service_count = 0;
    char** service_names = NULL;

    debug_printf("Enumerating before registering\n");
    nameserver_enumerate(&service_count, &service_names);
    debug_printf("Available services: %d\n", service_count);
    for (int i = 0; i < service_count; i++) {
    	debug_printf("\t%s\n", service_names[i]);
    }

    debug_printf("Registering service\n");
    nameserver_register("dummy_service", &own_rpc);

    // Handle requests
    debug_printf("Looping forever\n");
    aos_rpc_accept(&own_rpc);

    return SYS_ERR_OK;
}

