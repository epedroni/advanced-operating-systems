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
	//TODO: We have to think of a way how to provide refill function to every application
	return SYS_ERR_OK;
}

int main(int argc, char *argv[])
{
    int i=0;
    for(;i<argc;++i){
        printf("Printing argument: %d %s\n",i, argv[i]);
    }

    return 0;
}
