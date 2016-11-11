/**
 * \file
 * \brief Hello world application
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

struct aos_rpc *init_rpc;

errval_t aos_slab_refill(struct slab_allocator *slabs){
	debug_printf("Aos slab refill!\n");
	//TODO: We have to think of a way how to provide refill function to every application
	return SYS_ERR_OK;
}
//
//static errval_t allocate_ram(void){
//	debug_printf("Allocating slot\n");
//	struct capref frame_cap;
//	ERROR_RET1(slot_alloc(&frame_cap));
//	debug_printf("Retyping ram to frame\n");
//
//	struct capref ram_cap={
//			.cnode =cnode_base,
//			.slot=0
//	};
//	ERROR_RET1(cap_retype(frame_cap, ram_cap, 0,
//			ObjType_Frame, BASE_PAGE_SIZE, 1));
//
//	return SYS_ERR_OK;
//}

int main(int argc, char *argv[])
{
	debug_printf("Received %d arguments \n",argc);
	for(int i=0;i<argc;++i){
		debug_printf("Printing argument: %d %s\n",i, argv[i]);
	}
	errval_t err;

	// FIXME this isnt working
//	errval_t err=allocate_ram();
//	if(err_is_fail(err)){
//		DEBUG_ERR(err, "Failed to allocate RAM");
//	}

	init_rpc = get_init_rpc();
	debug_printf("init rpc: 0x%x\n", init_rpc);
	err = aos_rpc_send_number(get_init_rpc(), (uintptr_t)42);
	if(err_is_fail(err)){
		DEBUG_ERR(err, "Could not send number");
	}

	err = aos_rpc_send_string(get_init_rpc(), "milan, hello this is dog! :) hahahhahahahahahahahahaha\n");
	if(err_is_fail(err)){
		DEBUG_ERR(err, "Could not send simple string");
	}

	// get all pids, there should be two (init and us)
	domainid_t *pidptr = malloc(sizeof(domainid_t) * 10);
	uint32_t pidcount;
	err = aos_rpc_process_get_all_pids(get_init_rpc(), &pidptr, &pidcount);
	if(err_is_fail(err)){
		DEBUG_ERR(err, "Could not get PIDs");
	}
	for (int i = 0; i < pidcount; i++) {
		debug_printf("Received PID: %d\n", pidptr[i]);
	}

	// spawn memeater
	debug_printf("Spawning memeater via RPC from hello\n");
	domainid_t new_pid;
	err = aos_rpc_process_spawn(get_init_rpc(), "/armv7/sbin/memeater", 0, &new_pid);
	if(err_is_fail(err)){
		DEBUG_ERR(err, "Could not spawn memeater");
	}

	// get all pids again, there should be three now
	debug_printf("Getting the name of each running process\n");
	char *nameptr = malloc(sizeof(char) * 30);
	err = aos_rpc_process_get_all_pids(get_init_rpc(), &pidptr, &pidcount);
	if(err_is_fail(err)){
		DEBUG_ERR(err, "Could not get PIDs");
	}

	for (int i = 0; i < pidcount; i++) {
		err = aos_rpc_process_get_name(get_init_rpc(), pidptr[i], &nameptr);
		if(err_is_fail(err)){
			DEBUG_ERR(err, "Could not get domain name");
		}
		debug_printf("PID: %d, name: \"%s\"\n", pidptr[i], nameptr);
	}

	debug_printf("Trying to get the name of a PID that does not exist\n");
	err = aos_rpc_process_get_name(get_init_rpc(), 1000, &nameptr);
	if(err_is_fail(err)){
		debug_printf("Could not get domain name, that is expected\n");
	}
	debug_printf("PID: %d, name: \"%s\"\n", 1000, nameptr);

	debug_printf("Trying to spawn a domain that does not exist\n");
	err = aos_rpc_process_spawn(get_init_rpc(), "/armv7/sbin/fail", 0, &new_pid);
	if(err_is_fail(err)){
		debug_printf("Could not spawn domain, that is expected\n");
	}
	debug_printf("Returned PID: %d\n", new_pid);

	debug_printf("Waiting for memeater to return\n");
	while (!err_is_fail(aos_rpc_process_get_name(get_init_rpc(), 2, &nameptr)));

	debug_printf("Memeater appears to have returned, check all pids\n");
	err = aos_rpc_process_get_all_pids(get_init_rpc(), &pidptr, &pidcount);
    if(err_is_fail(err)){
        DEBUG_ERR(err, "Could not get PIDs");
    }
    for (int i = 0; i < pidcount; i++) {
        err = aos_rpc_process_get_name(get_init_rpc(), pidptr[i], &nameptr);
        if(err_is_fail(err)){
            DEBUG_ERR(err, "Could not get domain name");
        }
        debug_printf("PID: %d, name: \"%s\"\n", pidptr[i], nameptr);
    }

	free(nameptr);
	free(pidptr);

	while(true){
		char ret_char;
		aos_rpc_serial_getchar(get_init_rpc(),&ret_char);
		if(ret_char=='\r')
			ret_char='\n';
		if(ret_char!=0)
			aos_rpc_serial_putchar(get_init_rpc(),ret_char);
	}

	return 0;
}
