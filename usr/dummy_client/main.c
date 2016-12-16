/**
 * \file
 * \brief Blink service application
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

struct aos_rpc ns_rpc;

int main(int argc, char *argv[])
{
	size_t service_count = 0;
	char** service_names = NULL;

	debug_printf("Enumerating\n");
	nameserver_enumerate(&service_count, &service_names);
	debug_printf("Available services: %d\n", service_count);
	for (int i = 0; i < service_count; i++) {
		debug_printf("\t%s\n", service_names[i]);
	}

    debug_printf("Attempting to bind with dummy_service via nameserver\n");
    struct aos_rpc ds_rpc;
    nameserver_lookup("dummy_service", &ds_rpc);

    debug_printf("Trying some communication with the dummy_service\n");
    aos_rpc_send_string(&ds_rpc, "Hello this is dummy");

    return SYS_ERR_OK;
}

