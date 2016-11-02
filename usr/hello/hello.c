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

static errval_t allocate_ram(void){
    debug_printf("Allocating slot\n");
    struct capref frame_cap;
    ERROR_RET1(slot_alloc(&frame_cap));
    debug_printf("Retyping ram to frame\n");

    struct capref ram_cap={
        .cnode =cnode_base,
        .slot=0
    };
    ERROR_RET1(cap_retype(frame_cap, ram_cap, 0,
                    ObjType_Frame, BASE_PAGE_SIZE, 1));

    return SYS_ERR_OK;
}

int main(int argc, char *argv[])
{
    debug_printf("Received %d arguments \n",argc);
    for(int i=0;i<argc;++i){
        debug_printf("Printing argument: %d %s\n",i, argv[i]);
    }

    errval_t err=allocate_ram();

    init_rpc = get_init_rpc();
    debug_printf("init rpc: 0x%x\n", init_rpc);
    aos_rpc_send_number(get_init_rpc(), (uintptr_t)42);

    aos_rpc_send_string(get_init_rpc(), "milan, hello this is dog! :) hahahhahahahahahahahahaha\n");

    // here we spawn memeater, check that its PID is returned and check that it is actually memeater
    // FIXME these local buffers are terrible
    // 100 = magic
    domainid_t pids[100];
    domainid_t *pidptr = &pids[0];
    uint32_t pidcount;
    aos_rpc_process_get_all_pids(get_init_rpc(), &pidptr, &pidcount);
	for (int i = 0; i < pidcount; i++) {
		debug_printf("Received PID: %d\n", pids[i]);
	}

    debug_printf("Spawning memeater via RPC from hello\n");
    domainid_t new_pid;
    aos_rpc_process_spawn(get_init_rpc(), "/armv7/sbin/memeater", 0, &new_pid);

    aos_rpc_process_get_all_pids(get_init_rpc(), &pidptr, &pidcount);
	for (int i = 0; i < pidcount; i++) {
		debug_printf("New received PID: %d\n", pids[i]);
	}

    char name[100];
	char *nameptr = &name[0];
	debug_printf("Trying to get the name of the process associated with PID %d\n", new_pid);
	aos_rpc_process_get_name(get_init_rpc(), new_pid, &nameptr);
	debug_printf("The name of process %d is %s\n", new_pid, name);

    while(true){
        char ret_char;
        aos_rpc_serial_getchar(get_init_rpc(),&ret_char);
        if(ret_char=='\r')
            ret_char='\n';
        if(ret_char!=0)
            aos_rpc_serial_putchar(get_init_rpc(),ret_char);
    }

    if(err_is_fail(err)){
        DEBUG_ERR(err, "Failed initializing ram");
    }

    return 0;
}
