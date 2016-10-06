/**
 * \file
 * \brief AOS paging helpers.
 */

/*
 * Copyright (c) 2012, 2013, 2016, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetstr. 6, CH-8092 Zurich. Attn: Systems Group.
 */

#include <aos/aos.h>
#include <aos/paging.h>
#include <aos/except.h>
#include <aos/slab.h>
#include "threads_priv.h"
#include <mm/mm.h>

#include <stdio.h>
#include <string.h>

static struct paging_state current;

/**
 * \brief Helper function that allocates a slot and
 *        creates a ARM l2 page table capability
 */
__attribute__((unused))
static errval_t arml2_alloc(struct paging_state * st, struct capref *ret)
{
    errval_t err;
    err = st->slot_alloc->alloc(st->slot_alloc, ret);
    if (err_is_fail(err)) {
        debug_printf("slot_alloc failed: %s\n", err_getstring(err));
        return err;
    }
    err = vnode_create(*ret, ObjType_VNode_ARM_l2);
    if (err_is_fail(err)) {
        debug_printf("vnode_create failed: %s\n", err_getstring(err));
        return err;
    }
    return SYS_ERR_OK;
}

errval_t paging_init_state(struct paging_state *st, lvaddr_t start_vaddr,
        struct capref pdir, struct slot_allocator * ca)
{
    debug_printf("paging_init_state\n");
    // TODO (M2): implement state struct initialization
    // TODO (M4): Implement page fault handler that installs frames when a page fault
    // occurs and keeps track of the virtual address space.
    return SYS_ERR_OK;
}

/**
 * \brief This function initializes the paging for this domain
 * It is called once before main.
 */
errval_t paging_init(void)
{
    debug_printf("paging_init\n");
    // TODO (M2): Call paging_init_state for &current
    // TODO (M4): initialize self-paging handler
    // TIP: use thread_set_exception_handler() to setup a page fault handler
    // TIP: Think about the fact that later on, you'll have to make sure that
    // you can handle page faults in any thread of a domain.
    // TIP: it might be a good idea to call paging_init_state() from here to
    // avoid code duplication.
    current.slot_alloc = get_default_slot_allocator();
    set_current_paging_state(&current);
    return SYS_ERR_OK;
}


/**
 * \brief Initialize per-thread paging state
 */
void paging_init_onthread(struct thread *t)
{
    // TODO (M4): setup exception handler for thread `t'.
}

/**
 * \brief return a pointer to a bit of the paging region `pr`.
 * This function gets used in some of the code that is responsible
 * for allocating Frame (and other) capabilities.
 */
errval_t paging_region_init(struct paging_state *st, struct paging_region *pr, size_t size)
{
    void *base;
    errval_t err = paging_alloc(st, &base, size);
    if (err_is_fail(err)) {
        debug_printf("paging_region_init: paging_alloc failed\n");
        return err_push(err, LIB_ERR_VSPACE_MMU_AWARE_INIT);
    }
    pr->base_addr    = (lvaddr_t)base;
    pr->current_addr = pr->base_addr;
    pr->region_size  = size;
    // TODO: maybe add paging regions to paging state?
    return SYS_ERR_OK;
}

/**
 * \brief return a pointer to a bit of the paging region `pr`.
 * This function gets used in some of the code that is responsible
 * for allocating Frame (and other) capabilities.
 */
errval_t paging_region_map(struct paging_region *pr, size_t req_size,
                           void **retbuf, size_t *ret_size)
{
    lvaddr_t end_addr = pr->base_addr + pr->region_size;
    ssize_t rem = end_addr - pr->current_addr;
    if (rem > req_size) {
        // ok
        *retbuf = (void*)pr->current_addr;
        *ret_size = req_size;
        pr->current_addr += req_size;
    } else if (rem > 0) {
        *retbuf = (void*)pr->current_addr;
        *ret_size = rem;
        pr->current_addr += rem;
        debug_printf("exhausted paging region, "
                "expect badness on next allocation\n");
    } else {
        return LIB_ERR_VSPACE_MMU_AWARE_NO_SPACE;
    }
    return SYS_ERR_OK;
}

/**
 * \brief free a bit of the paging region `pr`.
 * This function gets used in some of the code that is responsible
 * for allocating Frame (and other) capabilities.
 * NOTE: Implementing this function is optional.
 */
errval_t paging_region_unmap(struct paging_region *pr, lvaddr_t base, size_t bytes)
{
    // XXX: should free up some space in paging region, however need to track
    //      holes for non-trivial case
    return SYS_ERR_OK;
}

/**
 * TODO(M2): Implement this function
 * \brief Find a bit of free virtual address space that is large enough to
 *        accomodate a buffer of size `bytes`.
 */
errval_t paging_alloc(struct paging_state *st, void **buf, size_t bytes)
{
    *buf = NULL;
    return SYS_ERR_OK;
}

/**
 * \brief map a user provided frame, and return the VA of the mapped
 *        frame in `buf`.
 */
errval_t paging_map_frame_attr(struct paging_state *st, void **buf,
                               size_t bytes, struct capref frame,
                               int flags, void *arg1, void *arg2)
{
    errval_t err = paging_alloc(st, buf, bytes);
    if (err_is_fail(err)) {
        return err;
    }
    return paging_map_fixed_attr(st, (lvaddr_t)(*buf), frame, bytes, flags);
}

errval_t
slab_refill_no_pagefault(struct slab_allocator *slabs, struct capref frame, size_t minbytes)
{
    // Refill the two-level slot allocator without causing a page-fault
    return SYS_ERR_OK;
}

/**
 * \brief map a user provided frame at user provided VA.
 * TODO(M1): Map a frame assuming all mappings will fit into one L2 pt
 * TODO(M2): General case
 */
errval_t paging_map_fixed_attr(struct paging_state *st, lvaddr_t vaddr,
        struct capref frame, size_t bytes, int flags)
{
	debug_printf("Entering: paging_map_fixed_attr\n");

    // TODO:
    // Copy if partially mapping large frames (cf Milestone1.pdf)
    // cap_copy(struct capref dest, struct capref src)
    // 1. Create L2
    // TODO: Don't overwrite existing L2
    // Keep it in paging_state
    struct capref l2_cap;
    errval_t err = arml2_alloc(st, &l2_cap);
    MM_ASSERT(err, "paging_map_fixed_attr: arml2_alloc failed");

    // 2. Map L2 to L1
    struct capref l1_pagetable = {
        .cnode = cnode_page,
        .slot = 0,
    };

    capaddr_t l1_slot=ARM_L1_OFFSET(vaddr);
    struct capref mapping_l2_to_l1;
    debug_printf("L1 slot: 0x%X\n",l1_slot);
    err = st->slot_alloc->alloc(st->slot_alloc, &mapping_l2_to_l1);

    struct frame_identity fi;
    err = frame_identify(frame, &fi);

    debug_printf("Frame address is: 0x%X of size: %lu\n", fi.base, fi.bytes/1024/1024);

    debug_printf("Adding L2 ot to L1 pt slot no: %d, \n",l1_slot);

    MM_ASSERT(err, "paging_map_fixed_attr: slot_alloc::alloc (1) failed");
    err = vnode_map(l1_pagetable, l2_cap,
    		l1_slot, VREGION_FLAGS_READ_WRITE,
            0, 0, mapping_l2_to_l1);
    MM_ASSERT(err, "paging_map_fixed_attr: vnode_map (1) failed");

    // 3. Map Frame to L2
    // TODO: Handle bad aligned vaddr?
    // TODO: Fill several slots if bytes > BASE_PAGE_SIZE
    capaddr_t l2_slot=ARM_L2_OFFSET(vaddr);
    debug_printf("Adding Frame ot to L2 pt slot no: %d, \n",l2_slot);
    struct capref mapping_frame_to_l2;
    err = st->slot_alloc->alloc(st->slot_alloc, &mapping_frame_to_l2);
    MM_ASSERT(err, "paging_map_fixed_attr: slot_alloc::alloc (2) failed");
    debug_printf("L2 slot: 0x%X\n",l2_slot);
    err = vnode_map(l2_cap, frame,
    		l2_slot, flags,
            0, 0, mapping_frame_to_l2);


    MM_ASSERT(err, "paging_map_fixed_attr: vnode_map (2) failed");
    return SYS_ERR_OK;
}

/**
 * \brief unmap a user provided frame, and return the VA of the mapped
 *        frame in `buf`.
 * NOTE: Implementing this function is optional.
 */
errval_t paging_unmap(struct paging_state *st, const void *region)
{
    return SYS_ERR_OK;
}

void test_paging(void)
{
    struct capref cap_ram;
    debug_printf("test_paging: Allocating RAM...\n");
    errval_t err = ram_alloc_fixed(&cap_ram, BASE_PAGE_SIZE, BASE_PAGE_SIZE);
    MM_ASSERT(err, "test_paging: ram_alloc_fixed");

    debug_printf("test_paging: Retype RAM -> Frame...\n");
    struct capref cap_as_frame;
    err = current.slot_alloc->alloc(current.slot_alloc, &cap_as_frame);
    MM_ASSERT(err, "test_paging: slot_alloc");
    err = cap_retype(cap_as_frame, cap_ram, 0,
        ObjType_Frame, BASE_PAGE_SIZE, 1);
    MM_ASSERT(err, "test_paging: cap_retype");

    static const lvaddr_t WRITING_ADDRESS=0x202000;

    debug_printf("test_paging: Paging frame...\n");
    int* addr = (int*)(WRITING_ADDRESS);
    err = paging_map_fixed_attr(&current, (lvaddr_t)(addr),
            cap_as_frame, BASE_PAGE_SIZE, VREGION_FLAGS_READ_WRITE);
    MM_ASSERT(err, "test_paging: paging_map_fixed_attr");

    debug_printf("test_paging: Writing to address 0x%x...\n", addr);
    *addr = 42;

    debug_printf("test_paging: Reading: %u :)\n", *addr);
}
