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

void test_allocate_frame(size_t alloc_size, struct capref* cap_as_frame);
void* test_alloc_and_map(size_t alloc_size);
void runtests_mem_alloc(void);
void test_paging(void);

/*
 * Server receive callback
 * The server does not need to spontaneously send messages,
 * all it does is respond to received messages with this function.
 */
//static void rcv_callback(void* args){
//    errval_t err;
//
//    debug_printf("** Server is receiving request\n");
//    struct lmp_chan* lc=(struct lmp_chan*)args;
//    struct lmp_recv_msg message = LMP_RECV_MSG_INIT;
//    struct capref child_endpoint;
//
//    lmp_chan_recv(lc, &message, &child_endpoint);
//
//    uint32_t ret_opcode = RPC_NULL_OPCODE;
//
//    debug_printf("We have message type: 0x%X\n",message.words[0]);
//
//    uint32_t  RPC_HEADER_OPCODE(message.words[0]);
//
//    uint32_t opcode = RPC_HEADER_OPCODE(message.words[0]);
//    // Mark that we need to send ack with next response
//    // Calling registered callback
//    switch(opcode) {
//    case RPC_HANDSHAKE:
//        debug_printf("Received handshake message!\n");
//        lc->remote_cap=child_endpoint;
//    	break;
//    case RPC_RAM_CAP:
//    	// create ram
//        ret_opcode = RPC_RAM_CAP;
//    	break;
//    case RPC_NUMBER:
//        debug_printf("We received a number: %d\n", message.words[1]);
//    	break;
//    case RPC_STRING:
//    	debug_printf("We received a string: %s \n", message.words+1);
//    	break;
//    case RPC_PUT_CHAR:
//    	break;
//    case RPC_GET_CHAR:
//    	break;
//    case RPC_SPAWN:
//    	break;
//    case RPC_GET_NAME:
//    	break;
//    case RPC_GET_PID:
//    	break;
//    default:
//    	break;
//    }
//
//    debug_printf("Reregistring, ofcourse \n");
//    MM_ASSERT(lmp_chan_alloc_recv_slot(lc), "Allocating slot for receive");
//
//    // If we need to send ACK...
//    lmp_chan_register_recv(lc, get_default_waitset(), MKCLOSURE(rcv_callback, args));
//
//    err=lmp_chan_send1(lc,
//        LMP_FLAG_SYNC,
//        NULL_CAP,
//        MAKE_RPC_MSG_HEADER(ret_opcode, RPC_FLAG_ACK));
//    if(err_is_fail(err)){
//        debug_printf("Message not sent\n");
//    }

//}

#define RPC_BUFF_SIZE 100
static char rpc_rcv_buffer[RPC_BUFF_SIZE];

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

    err = paging_init();
    if(err_is_fail(err)){
        DEBUG_ERR(err, "paging_init");
    }

    err = slot_alloc_init();
    if(err_is_fail(err)){
        DEBUG_ERR(err, "slot_alloc_init");
    }

    // Retype dispatcher to endpoint
    ERROR_RET1(cap_retype(cap_selfep, cap_dispatcher, 0,
        ObjType_EndPoint, 0, 1));
    //Create lmp channel
    struct spawninfo* process_info;

    //Spawn child
    struct aos_rpc rpc;
    aos_rpc_init(&rpc, NULL_CAP, false);

    process_info = malloc(sizeof(struct spawninfo));
    process_info->core_id=my_core_id;   //Run it on same core
    err = spawn_load_by_name("/armv7/sbin/hello", process_info, &rpc.lc);
    if(err_is_fail(err)){
        DEBUG_ERR(err, "spawn_load_by_name");
    }
    free(process_info);

//  spawn another for kicks
//    struct lmp_chan lc2;
//	MM_ASSERT(lmp_chan_accept(&lc2, DEFAULT_LMP_BUF_WORDS, NULL_CAP), "Error creating lmp channel");
//	MM_ASSERT(lmp_chan_alloc_recv_slot(&lc2), "Allocating slot for receive");
//	ERROR_RET1(lmp_chan_register_recv(&lc2, get_default_waitset(), MKCLOSURE(rcv_callback, &lc2)));
//	process_info = malloc(sizeof(struct spawninfo));
//	process_info->core_id=my_core_id;   //Run it on same core
//	err = spawn_load_by_name("/armv7/sbin/memeater", process_info, &lc2);
//	if(err_is_fail(err)){
//		DEBUG_ERR(err, "spawn_load_by_name");
//	}
//	free(process_info);

    debug_printf("Message handler loop\n");
    //#define LOGO(s) debug_printf("%s\n", s);
    #define LOGO(s)
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


    struct lmp_server_state lmp_state={
            .buffer=rpc_rcv_buffer,
            .buffer_capacity=RPC_BUFF_SIZE
    };
    lmp_server_init(&rpc, &lmp_state);

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

    PRINT_TEST("Unmap test");
    number = (int*)test_alloc_and_map(BASE_PAGE_SIZE);
    *number = 42;
    errval_t err = paging_unmap(get_current_paging_state(), (void*)number);
    MM_ASSERT(err, "unmap");
    // Un-comment the 2 following lines to test.
    // But kind of destructive test :p
    //debug_printf("Should crash now\n");
    //*number = 1;

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
		debug_printf("Reading byte from page: %d it should be %d and it is: %d, seems: %s\n", i+1, i+42, number[i],(i+42==number[i])?"ok":"not ok");

}
