/**
 * \file
 * \brief ram allocator functions
 */

/*
 * Copyright (c) 2016, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetsstrasse 6, CH-8092 Zurich. Attn: Systems Group.
 */

#ifndef _INIT_MEM_ALLOC_H_
#define _INIT_MEM_ALLOC_H_

#include <stdio.h>
#include <aos/aos.h>

extern struct mm aos_mm;
extern struct bootinfo *bi;

errval_t initialize_ram_alloc(void);
errval_t aos_init_mm(void);
errval_t aos_ram_free(struct capref cap, size_t bytes);
void* get_mapped_page(size_t* alloc_size);

#endif /* _INIT_MEM_ALLOC_H_ */
