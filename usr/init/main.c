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

    if (my_core_id == 0)
    {
        domainid_t pid;
        debug_printf("Spawning networking\n");
        ERR_CHECK("spawning networking", processmgr_spawn_process("/armv7/sbin/networking", 0, &pid));
//
//        struct capref uart4_frame;
//        slot_alloc(&uart4_frame);
//        ERR_CHECK("Forging UART4 frame", frame_forge(uart4_frame, OMAP44XX_MAP_L4_PER_UART4, OMAP44XX_MAP_L4_PER_UART4_SIZE, 0))
//
//        void* uart_address=NULL;
//        ERR_CHECK("mapping uart frame", paging_map_frame_attr(get_current_paging_state(), &uart_address,
//                OMAP44XX_MAP_L4_PER_UART4_SIZE, uart4_frame, VREGION_FLAGS_READ_WRITE | VREGION_FLAGS_NOCACHE, NULL, NULL));
//
//        ERR_CHECK("Init serial",serial_init((lvaddr_t)uart_address, UART4_IRQ));
    }

    os_core_events_loop();

    return EXIT_SUCCESS;
}
