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

#include <spawn/spawn.h>
#include <aos/aos_rpc.h>

coreid_t my_core_id;
struct bootinfo *bi;

struct paging_test
{
    uint32_t num;
};

static struct paging_test test;

static struct aos_rpc rpc;

void test_allocate_frame(size_t alloc_size, struct capref* cap_as_frame);
void* test_alloc_and_map(size_t alloc_size);
void runtests_mem_alloc(void);
void test_paging(void);

int main(int argc, char *argv[])
{
    errval_t err;

    /// /sbin/init initialization sequence.
    // Warning: order of steps MATTERS
    debug_printf("main() being invoked\n");

    // 1. Find core ID
    err = invoke_kernel_get_core_id(cap_kernel, &my_core_id);
    assert(err_is_ok(err));
    disp_set_core_id(my_core_id);
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
    runtests_mem_alloc();
    test_paging();

    // Test spawn a process
    if (my_core_id == 0)
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

void runtests_mem_alloc(void)
{
    debug_printf("Running mem_alloc tests set\n");
    struct frame_identity frame_id;

    #define TEST_ALLOC(size, cap) {\
        MM_ASSERT(ram_alloc(&cap, size), "Alloc failed");\
        frame_identify(cap, &frame_id);\
        /*debug_printf("\tRAM [capslot %u]: addr 0x%08x, size 0x%08x\n",*/\
            /*cap.slot, (int)frame_id.base, (int)frame_id.bytes);*/\
    }

    struct capref ref;
    debug_printf("\tAllocating a LOT of small pages\n");
    for (int i = 0; i < 500; ++i)
        TEST_ALLOC(BASE_PAGE_SIZE, ref);


    // Keep it simple for now, allocate 4kB caps
    int alloc_size = BASE_PAGE_SIZE;
    debug_printf("Allocate 5x%u bits...\n", alloc_size);
    struct capref smallCaps[5];
    for (int i = 0; i < 5; ++i)
       TEST_ALLOC(alloc_size, smallCaps[i]);

    // Test large pages now - 1MB
    struct capref largeCaps[5];
    alloc_size = LARGE_PAGE_SIZE;
    debug_printf("Allocate 5x%u bits...\n", alloc_size);
    for (int i = 0; i < 5; ++i)
        TEST_ALLOC(alloc_size, largeCaps[i]);

    // Let's test merging now - if we free the original 5 caps and try to allocate something like 16kB, those nodes should be merged and used
    alloc_size = BASE_PAGE_SIZE;
    debug_printf("Freeing 5x%u bits...\n", alloc_size);
    for (int i = 0; i < 5; ++i)
        MM_ASSERT(aos_ram_free(smallCaps[i], alloc_size), "aos_ram_free");

    struct capref mediumCap;
    alloc_size = BASE_PAGE_SIZE * 4;
    debug_printf("Allocate 1x%u bits...\n", alloc_size);
    TEST_ALLOC(alloc_size, mediumCap)

    // Now free everything and check if we are back to the single ram chunk again
    alloc_size = LARGE_PAGE_SIZE;
    debug_printf("Freeing 5x%u bits...\n", alloc_size);
    for (int i = 0; i < 5; ++i)
        MM_ASSERT(aos_ram_free(largeCaps[i], alloc_size), "mm_free failed");

    alloc_size = BASE_PAGE_SIZE * 4;
    debug_printf("\tFreeing medium-sized alloc\n");
    MM_ASSERT(aos_ram_free(mediumCap, alloc_size), "mm_free failed");
}

void test_allocate_frame(size_t alloc_size, struct capref* cap_as_frame) {
    struct paging_state* paging_state =get_current_paging_state();

    struct capref cap_ram;
    debug_printf("test_allocate_frame: Allocating RAM...\n");
    errval_t err = ram_alloc(&cap_ram, alloc_size);
    MM_ASSERT(err, "test_allocate_frame: ram_alloc_fixed");

    err = paging_state->slot_alloc->alloc(paging_state->slot_alloc, cap_as_frame);
    err = cap_retype(*cap_as_frame, cap_ram, 0,
            ObjType_Frame, alloc_size, 1);
    MM_ASSERT(err, "test_allocate_frame: cap_retype");
}

void* test_alloc_and_map(size_t alloc_size) {
    struct paging_state* paging_state =get_current_paging_state();
    struct capref cap_as_frame;
    test_allocate_frame(alloc_size, &cap_as_frame);

    void* address=NULL;
    errval_t err = paging_map_frame_attr(paging_state,&address,alloc_size,
            cap_as_frame,VREGION_FLAGS_READ_WRITE,NULL,NULL);
    MM_ASSERT(err, "get_page: paging_map_fixed_attr");

    return address;
}

void test_paging(void)
{
    #define PRINT_TEST(title) debug_printf("###########################\n"); debug_printf("#TEST%02u: %s\n", ++test.num, title);

    int* number;


    PRINT_TEST("Allocate and map one page");
    void* page = test_alloc_and_map(BASE_PAGE_SIZE);
    number = (int*)page;
    *number=42;

    PRINT_TEST("Allocate and map 2 pages");
    page = test_alloc_and_map(2 * BASE_PAGE_SIZE);
    number = (int*)page;
    for (int i = 0; i < (2 * BASE_PAGE_SIZE) / sizeof(int); ++i)
       number[i] = i;

    PRINT_TEST("Allocate pages to check allocater reuses l2 page table correctly");
    int *numbers[5];

    int i=0;
    for (;i<5;i++){
        void* page1=test_alloc_and_map(BASE_PAGE_SIZE);
        numbers[i]=(int*)page1;
        *numbers[i]=42+i;
    }

    for (i=0; i<5; i++){
        debug_printf("%d\n",*numbers[i]);
    }

    PRINT_TEST("Allocate big big page (over several L2)");
    number = (int*)test_alloc_and_map(5 * LARGE_PAGE_SIZE);
    for (i = 0; i < 5*LARGE_PAGE_SIZE / sizeof(int); i+=BASE_PAGE_SIZE)
        number[i] = 42+i;

    for (i = 0; i < 5*LARGE_PAGE_SIZE / sizeof(int); i+=BASE_PAGE_SIZE)
        if (i+42 != number[i])
              debug_printf("Reading byte from page: %d it should be %d and it is: %d\n", i+1, i+42, number[i]);

    PRINT_TEST("Unmap test.");
    number = (int*)test_alloc_and_map(BASE_PAGE_SIZE);
    *number = 42;
    errval_t err = paging_unmap(get_current_paging_state(), (void*)number);
    MM_ASSERT(err, "unmap");
    // Un-comment the 2 following lines to test.
    // But kind of destructive test :p
    //debug_printf("Should crash now\n");
    //*number = 1;
}
