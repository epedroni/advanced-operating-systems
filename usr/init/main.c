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
#include <aos/waitset.h>
#include <aos/morecore.h>
#include <aos/paging.h>

#include <mm/mm.h>
#include "mem_alloc.h"
#include "lrpc_server.h"
#include "coreboot.h"
#include "processmgr.h"
#include "tests.h"

#include <spawn/spawn.h>
#include <aos/aos_rpc.h>

coreid_t my_core_id;
struct bootinfo *bi;

static struct aos_rpc rpc;

int main(int argc, char *argv[])
{
    errval_t err;

    /// /sbin/init initialization sequence.
    // Warning: order of steps MATTERS

    // 1. Find core ID
    err = invoke_kernel_get_core_id(cap_kernel, &my_core_id);
    assert(err_is_ok(err));
    disp_set_core_id(my_core_id);
    debug_printf("main() being invoked\n"); // After set_core_id to properly display core id
    ERROR_RET1(cap_retype(cap_selfep, cap_dispatcher, 0,
        ObjType_EndPoint, 0, 1));

    // 2. Get boot info. Get it from args or read it from URPC
    bi = (struct bootinfo*)strtol(argv[1], NULL, 10);
    if (!bi) {
        assert(my_core_id > 0);
        bi=malloc(sizeof(struct bootinfo)+(sizeof(struct mem_region)*2));
        memset(bi, 0, sizeof(struct bootinfo)+(sizeof(struct mem_region)*2));

        //TODO: Read this from arguments
        struct frame_identity urpc_frame_id;
        frame_identify(cap_urpc, &urpc_frame_id);
        void* urpc_buffer;
        err = paging_map_frame(get_current_paging_state(), &urpc_buffer, urpc_frame_id.bytes, cap_urpc,
                    NULL, NULL);
        if (err_is_fail(err))
            DEBUG_ERR(err, "paging_map_frame");

        err = read_from_urpc(urpc_buffer,&bi,1);
        if (err_is_fail(err))
            DEBUG_ERR(err, "read_from_urpc");

        err = read_modules(urpc_buffer,bi,1);
        if (err_is_fail(err))
            DEBUG_ERR(err, "read_modules");
    }


    // 3. Initialize RAM alloc. Requires a correct boot info.
    assert(bi);
    err = initialize_ram_alloc(my_core_id);
    if(err_is_fail(err))
        DEBUG_ERR(err, "initialize_ram_alloc");

    // 4. Init RPC server
    aos_rpc_init(&rpc, NULL_CAP, false);
    lmp_server_init(&rpc);
    processmgr_init(&rpc, argv[0]);

    // 5. Boot second core if needed
    if (my_core_id==0){
        debug_printf("--- Starting new core!\n");
        coreboot_init(bi);
    }

    #define LOGO(s) debug_printf("%s\n", s);
    LOGO(",-.----.                                                                                                   ");
    LOGO("\\    /  \\                                                             ,---,.                               ");
    LOGO("|   :    \\                              ,---,                       ,'  .'  \\                              ");
    LOGO("|   |  .\\ :                 ,---,     ,---.'|                     ,---.' .' |                      __  ,-. ");
    LOGO(".   :  |: |             ,-+-. /  |    |   | :                     |   |  |: |                    ,' ,'/ /| ");
    LOGO("|   |   \\ : ,--.--.    ,--.'|'   |    |   | |   ,--.--.           :   :  :  /   ,---.     ,---.  '  | |' | ");
    LOGO("|   : .   //       \\  |   |  ,\"' |  ,--.__| |  /       \\          :   |    ;   /     \\   /     \\ |  |   ,' ");
    LOGO(";   | |`-'.--.  .-. | |   | /  | | /   ,'   | .--.  .-. |         |   :     \\ /    /  | /    /  |'  :  /   ");
    LOGO("|   | ;    \\__\\/: . . |   | |  | |.   '  /  |  \\__\\/: . .         |   |   . |.    ' / |.    ' / ||  | '    ");
    LOGO(":   ' |    ,\" .--.; | |   | |  |/ '   ; |:  |  ,\" .--.; |         '   :  '; |'   ;   /|'   ;   /|;  : |    ");
    LOGO(":   : :   /  /  ,.  | |   | |--'  |   | '/  ' /  /  ,.  |         |   |  | ; '   |  / |'   |  / ||  , ;    ");
    LOGO("|   | :  ;  :   .'   \\|   |/      |   :    :|;  :   .'   \\        |   :   /  |   :    ||   :    | ---'     ");
    LOGO("`---'.|  |  ,     .-./'---'        \\   \\  /  |  ,     .-./        |   | ,'    \\   \\  /  \\   \\  /           ");
    LOGO("  `---`   `--`---'                  `----'    `--`---'            `----'       `----'    `----'            ");
    LOGO("                                        ... Well actually we are simply TeamF. But we are still awesome ;)");
    // END OF INIT INITIALIZATION SEQUENCE
    // DONT BREAK THE ORDER OF THE CODE BEFORE, UNLESS YOU KNOW WHAT YOU ARE DOING

    // Run tests
    run_all_tests();

    // Test spawn a process
    if (my_core_id == 1)
    {
        domainid_t pid;
        err = spawn_process("/armv7/sbin/hello", &rpc, my_core_id, &pid);
        if (err_is_fail(err))
            DEBUG_ERR(err, "spawn_process");
    }

    debug_printf("Entering accept loop forever\n");
    aos_rpc_accept(&rpc);

    return EXIT_SUCCESS;
}
