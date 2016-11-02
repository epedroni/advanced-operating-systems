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

    // 100 bytes = magic
    char name[100];
    char *nameptr = &name[0];
    aos_rpc_process_get_name(get_init_rpc(), 100, &nameptr);
    debug_printf("My name is %s\n", name);

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
