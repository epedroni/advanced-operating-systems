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

static errval_t setup_endpoint(void){
        debug_printf("child_hello: lmp_endpoint_init\n");
        lmp_endpoint_init();

        debug_printf("Creating child endpoint\n");
        struct lmp_chan lmp_chan;

        ERROR_RET1(lmp_chan_accept(&lmp_chan, 10, cap_initep));
        debug_printf("child_hello: Endpoint created\n");
        return SYS_ERR_OK;
}

int main(int argc, char *argv[])
{
    debug_printf("Received %d arguments \n",argc);
    for(int i=0;i<argc;++i){
        debug_printf("Printing argument: %d %s\n",i, argv[i]);
    }

    allocate_ram();

    //Check if we can map ram caps passed from parent, needed for later paging
    errval_t err=setup_endpoint();
    if(err_is_fail(err)){
        DEBUG_ERR(err, "Failed initializing endpoint");
    }

    return 0;
}
