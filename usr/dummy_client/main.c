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
    debug_printf("Attempting to bind with dummy_service via nameserver\n");
    struct capref ret_cap;
    nameserver_lookup("dummy_service", &ret_cap);

    debug_printf("Received cap, initialising rpc with dummy_service\n");
    struct aos_rpc ds_rpc;
    aos_rpc_init(&ds_rpc, ret_cap, true);

    debug_printf("Trying some communication with the dummy_service\n");
    aos_rpc_send_string(&ds_rpc, "Hello this is dummy");



    return SYS_ERR_OK;
}

