/**
 * \file
 * \brief init process for child spawning
 */

/*
 * Copyright (c) 2007, 2008, 2009, 2010, 2016, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Haldeneggsteig 4, CH-8092 Zurich. Attn: Systems Group.
 */

#include <stdio.h>
#include <stdlib.h>
#include <aos/aos.h>

#include "tests.h"
#include "init.h"
#include "nameserver.h"
#include "process/processmgr.h"
#include <omap44xx_map.h>
#include <netutil/user_serial.h>
#include <aos/inthandler.h>

void serial_input(uint8_t *buf, size_t len){
    debug_printf("Received: %c\n", *buf);
}

int main(int argc, char *argv[])
{
    errval_t err = os_core_initialize(argc, argv);
    if (err_is_fail(err))
    {
        DEBUG_ERR(err, "Unable to initialize core!");
        return 0;
    }

    // Run tests
    run_all_tests();

    if (my_core_id == 0)
    {
        domainid_t pid;

        debug_printf("Spawning nameserver\n");
        ERR_CHECK("spawning nameserver", processmgr_spawn_process("/armv7/sbin/nameserver", 0, &pid));

        // nameserver needs to be finished before we continue spawning stuff
        finish_nameserver();

//        debug_printf("Spawning networking\n");
//        ERR_CHECK("spawning networking", processmgr_spawn_process("/armv7/sbin/networking", 0, &pid));

        debug_printf("Starting shell...\n");
        err = processmgr_spawn_process("/armv7/sbin/shell", 0, &pid);
        if (err_is_fail(err))
            DEBUG_ERR(err, "spawn_process");
    }

    os_core_events_loop();

    return EXIT_SUCCESS;
}

