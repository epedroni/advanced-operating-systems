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
#include <spawn/spawn.h>
#include "mem_alloc.h"
#include "lrpc_server.h"
#include <aos/aos_rpc.h>

coreid_t my_core_id;
struct bootinfo *bi;

struct paging_test
{
    struct mm* mm;
    uint32_t num;
};

static struct paging_test test;

struct running_process
{
    struct running_process *next, *prev;
    domainid_t pid;
    char *name;
};

static struct running_process *running_procs = NULL;
static uint32_t running_count = 0;
static struct aos_rpc rpc;

void test_allocate_frame(size_t alloc_size, struct capref* cap_as_frame);
void* test_alloc_and_map(size_t alloc_size);
void runtests_mem_alloc(void);
void test_paging(void);

static
errval_t spawn_process(char* process_name, domainid_t *ret_pid){
    errval_t err;
    struct aos_rpc_session* sess = NULL;
    aos_server_add_client(&rpc, &sess);

    struct spawninfo* process_info = malloc(sizeof(struct spawninfo));
    process_info->core_id=my_core_id;   //Run it on same core
    err = spawn_load_by_name(process_name,
        process_info,
        &sess->lc);
    free(process_info);
    if(err_is_fail(err)) {
        *ret_pid = 0;
        return err;
    }

    aos_server_register_client(&rpc, sess);

    // add to running processes list
    struct running_process *rp = malloc(sizeof(struct running_process));
    rp->prev = NULL;
    rp->next = running_procs;
    rp->pid = running_count++;
    rp->name = process_name;

    running_procs = rp;

    *ret_pid = rp->pid;
    return SYS_ERR_OK;
}

// TODO these handlers need to go somewhere else
static
errval_t handle_get_name(struct aos_rpc_session* sess,
        struct lmp_recv_msg* msg,
        struct capref received_capref,
        struct capref* ret_cap,
        uint32_t* ret_type,
        uint32_t* ret_flags)
{
    assert(sess);

    struct running_process *rp = running_procs;
    domainid_t requested_pid = msg->words[1];
    while (rp && rp->pid != requested_pid) {
        rp = rp->next;
    }

    size_t size = 0;
    if (rp) {
        size = strlen(rp->name);
        if (size+1 > sess->shared_buffer_size)
            return RPC_ERR_BUF_TOO_SMALL;
        strcpy(sess->shared_buffer, rp->name);
    }

    ERROR_RET1(lmp_chan_send2(&sess->lc,
            LMP_FLAG_SYNC,
            NULL_CAP,
            MAKE_RPC_MSG_HEADER(RPC_GET_NAME, rp ? RPC_FLAG_ACK : RPC_FLAG_ERROR),
            size));

    return SYS_ERR_OK;
}

static
errval_t handle_get_pid(struct aos_rpc_session* sess,
        struct lmp_recv_msg* msg,
        struct capref received_capref,
        struct capref* ret_cap,
        uint32_t* ret_type,
        uint32_t* ret_flags)
{
    assert(sess);

    // should the running processes be kept in an array instead of a linked list?
    domainid_t pids[running_count];
    struct running_process *rp = running_procs;
    for (int i = 0; i < running_count && rp; i++) {
        pids[i] = rp->pid;
        rp = rp->next;
    }
    domainid_t *pidptr = &pids[0];

    if (running_count * sizeof(domainid_t) > sess->shared_buffer_size)
        return RPC_ERR_BUF_TOO_SMALL;

    memcpy(sess->shared_buffer, pidptr, running_count * sizeof(domainid_t));

    ERROR_RET1(lmp_chan_send2(&sess->lc,
            LMP_FLAG_SYNC,
            NULL_CAP,
            MAKE_RPC_MSG_HEADER(RPC_GET_PID, RPC_FLAG_ACK),
            running_count));

    return SYS_ERR_OK;
}

static
errval_t handle_spawn(struct aos_rpc_session* sess,
        struct lmp_recv_msg* msg,
        struct capref received_capref,
        struct capref* ret_cap,
        uint32_t* ret_type,
        uint32_t* ret_flags)
{
    if (!sess->shared_buffer_size)
        return RPC_ERR_SHARED_BUF_EMPTY;

    size_t string_size = msg->words[1];
    ASSERT_PROTOCOL(string_size <= sess->shared_buffer_size);

    char* process_name = malloc(string_size + 1);
    memcpy(process_name, sess->shared_buffer_size, string_size);
    process_name[string_size] = 0;

    domainid_t ret_pid;
    errval_t err = spawn_process(sess->shared_buffer, &ret_pid);

    ERROR_RET1(lmp_chan_send2(&sess->lc,
                LMP_FLAG_SYNC,
                NULL_CAP,
                MAKE_RPC_MSG_HEADER(RPC_SPAWN, (err_is_fail(err) ? RPC_FLAG_ERROR : RPC_FLAG_ACK)),
                ret_pid));

    return SYS_ERR_OK;
}

int main(int argc, char *argv[])
{
    debug_printf("MAIN IS BEING INVOKED\n");

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

    // Retype dispatcher to endpoint
    ERROR_RET1(cap_retype(cap_selfep, cap_dispatcher, 0,
        ObjType_EndPoint, 0, 1));
    //Create lmp channel
    runtests_mem_alloc();
    test_paging();

    // Init server
    aos_rpc_init(&rpc, NULL_CAP, false);

    // we are PID 0, add ourselves to the list
    struct running_process *init_rp = malloc(sizeof(struct running_process));
    init_rp->prev = NULL;
    init_rp->next = NULL;
    init_rp->pid = running_count++;
    init_rp->name = argv[0];
    running_procs = init_rp;

//    for (int i = 0; i < 20; ++i) {
    domainid_t pid;
    spawn_process("/armv7/sbin/hello", &pid);
//    }
//    spawn_process("/armv7/sbin/memeater", 20);

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
    LOGO("                                        ... Well actually we are simply TeamF. But we are still awesome ;)");
    // Hang around
    debug_printf("Starting lmp server...\n");


    lmp_server_init(&rpc);
    aos_rpc_register_handler(&rpc, RPC_GET_NAME, handle_get_name, false);
    aos_rpc_register_handler(&rpc, RPC_GET_PID, handle_get_pid, false);
    aos_rpc_register_handler(&rpc, RPC_SPAWN, handle_spawn, false);
    aos_rpc_accept(&rpc);

    return EXIT_SUCCESS;
}

void runtests_mem_alloc(void)
{
    debug_printf("Running mem_alloc tests set\n");
    struct frame_identity frame_id;
    struct mm* mm = mm_get_default();

    #define TEST_ALLOC(size, cap) {\
        MM_ASSERT(mm_alloc(mm, size, &cap), "Alloc failed");\
        frame_identify(cap, &frame_id);\
        debug_printf("\tRAM [capslot %u]: addr 0x%08x, size 0x%08x\n",\
            cap.slot, (int)frame_id.base, (int)frame_id.bytes);\
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
    errval_t err = mm_alloc(test.mm, alloc_size, &cap_ram);
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

    test.mm = mm_get_default();
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
