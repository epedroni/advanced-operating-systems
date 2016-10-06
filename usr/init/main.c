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

coreid_t my_core_id;
struct bootinfo *bi;

void runtests_mem_alloc(void);

int main(int argc, char *argv[])
{
    errval_t err;

    /* Set the core id in the disp_priv struct */
    err = invoke_kernel_get_core_id(cap_kernel, &my_core_id);
    assert(err_is_ok(err));
    disp_set_core_id(my_core_id);

    debug_printf("init: on core %" PRIuCOREID " invoked as:", my_core_id);
    for (int i = 0; i < argc; i++) {
       printf(" %s", argv[i]);
    }
    printf("\n");

    /* First argument contains the bootinfo location, if it's not set */
    bi = (struct bootinfo*)strtol(argv[1], NULL, 10);
    if (!bi) {
        assert(my_core_id > 0);
    }

    err = initialize_ram_alloc();
    if(err_is_fail(err)){
        DEBUG_ERR(err, "initialize_ram_alloc");
    }

    err = paging_init();
    if(err_is_fail(err)){
        DEBUG_ERR(err, "paging_init");
    }

    err = slot_alloc_init();
    if(err_is_fail(err)){
        DEBUG_ERR(err, "slot_alloc_init");
    }

    test_paging();
    runtests_mem_alloc();

    debug_printf("Message handler loop\n");
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

    // Hang around
    struct waitset *default_ws = get_default_waitset();
    while (true) {
        err = event_dispatch(default_ws);
        if (err_is_fail(err)) {
            DEBUG_ERR(err, "in event_dispatch");
            abort();
        }
    }

    return EXIT_SUCCESS;
}


void runtests_mem_alloc(void)
{
	debug_printf("\tAllocating a LOT of small pages\n");
    for (int i = 0; i < 500; ++i)
    {
        struct capref ref;
        errval_t err = mm_alloc(&aos_mm, BASE_PAGE_SIZE, &ref);
		MM_ASSERT(err, "Oups, error in mm_alloc");
    }

	debug_printf("Running mem_alloc tests set\n");
	struct mm* mm = mm_get_default();

	mm_print_nodes(mm);

	// Keep it simple for now, allocate 4kB caps
	int alloc_size = BASE_PAGE_SIZE;
	debug_printf("Allocate 5x%u bits...\n", alloc_size);
	struct capref smallCaps[5];
	for (int i = 0; i < 5; ++i)
	{
		errval_t err = mm_alloc(mm, alloc_size, &smallCaps[i]);
		MM_ASSERT(err, "Alloc failed");
		debug_printf("\tAllocated cap [0x%x] at slot %u. Ret %u\n", &smallCaps[i], smallCaps[i].slot, err);
	}

	// Test large pages now - 1MB
	struct capref largeCaps[5];
	alloc_size = LARGE_PAGE_SIZE;
	debug_printf("Allocate 5x%u bits...\n", alloc_size);
	for (int i = 0; i < 5; ++i)
	{
		errval_t err = mm_alloc(mm, alloc_size, &largeCaps[i]);
		MM_ASSERT(err, "Alloc failed");
		debug_printf("\tAllocated cap [0x%x] at slot %u. Ret %u\n", &largeCaps[i], largeCaps[i].slot, err);
	}

	// Let's test merging now - if we free the original 5 caps and try to allocate something like 16kB, those nodes should be merged and used
	alloc_size = BASE_PAGE_SIZE;
	debug_printf("Freeing 5x%u bits...\n", alloc_size);
	for (int i = 0; i < 5; ++i)
	{
		debug_printf("\tFreeing alloc #%u\n", i);
		MM_ASSERT(aos_ram_free(smallCaps[i], alloc_size), "mm_free failed");
	}

	struct capref mediumCap;
	alloc_size = BASE_PAGE_SIZE * 4;
	debug_printf("Allocate %u bits...\n", alloc_size);
	errval_t err = mm_alloc(mm, alloc_size, &mediumCap);
	MM_ASSERT(err, "Alloc failed");
	debug_printf("\tAllocated cap [0x%x] at slot %u. Ret %u\n", &mediumCap, mediumCap.slot, err);

	// Now free everything and check if we are back to the single ram chunk again
	alloc_size = LARGE_PAGE_SIZE;
	debug_printf("Freeing 5x%u bits...\n", alloc_size);
	for (int i = 0; i < 5; ++i)
	{
		debug_printf("\tFreeing alloc #%u\n", i);
		MM_ASSERT(aos_ram_free(largeCaps[i], alloc_size), "mm_free failed");
		debug_printf("Done\n");
	}

	alloc_size = BASE_PAGE_SIZE * 4;
	debug_printf("\tFreeing medium-sized alloc\n");
	MM_ASSERT(aos_ram_free(mediumCap, alloc_size), "mm_free failed");

	mm_print_nodes(mm);
}
