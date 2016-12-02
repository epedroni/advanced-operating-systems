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

// ???
errval_t aos_slab_refill(struct slab_allocator *slabs){
	return SYS_ERR_OK;
}

errval_t start_shell(void);

errval_t start_shell(void){
    debug_printf("Shell not yet implemented\n");
    return SYS_ERR_OK;
}

int main(int argc, char *argv[])
{
	start_shell();
	return 0;
}
