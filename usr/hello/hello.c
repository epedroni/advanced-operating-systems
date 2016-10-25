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

static errval_t setup_endpoint(void){
    debug_printf("child_hello: lmp_endpoint_init\n");
        lmp_endpoint_init();

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
        debug_printf("Creating child endpoint\n");
        struct lmp_endpoint* lmp_endpoint;
        struct capref endpoint_cap;
        debug_printf("child_hello: endpoint create\n");
        ERROR_RET1(endpoint_create(LMP_RECV_LENGTH, &endpoint_cap, &lmp_endpoint));

        return SYS_ERR_OK;
}

int main(int argc, char *argv[])
{
    printf("Received %d arguments \n",argc);
    int i=0;
    for(;i<argc;++i){
        printf("Printing argument: %d %s\n",i, argv[i]);
    }

    errval_t err=setup_endpoint();
    if(err_is_fail(err)){
        DEBUG_ERR(err, "Failed initializing endpoint");
    }

    return 0;
}
