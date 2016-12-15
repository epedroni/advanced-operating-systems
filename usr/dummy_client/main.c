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
    debug_printf("Binding with nameserver\n");
    nameserver_rpc_init(&ns_rpc);



    return SYS_ERR_OK;
}

