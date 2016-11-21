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
#include "processmgr.h"

int main(int argc, char *argv[])
{
    errval_t err = os_core_initialize(argc, argv);
    if (err_is_fail(err))
    {
        DEBUG_ERR(err, "Unable to initialize core!");
        return 0;
    }

    // Run tests
//    run_all_tests();

    // Test spawn a process
    if (my_core_id == 1 && false)
    {
        domainid_t pid;
        err = spawn_process("/armv7/sbin/hello", &rpc, my_core_id, &pid);
        if (err_is_fail(err))
            DEBUG_ERR(err, "spawn_process");
    }

    os_core_events_loop();

    return EXIT_SUCCESS;
}
