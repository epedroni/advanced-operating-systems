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
    st->slot_alloc=ca;
    memset(st->l2nodes, 0, sizeof(st->l2nodes));
    slab_init(&st->slabs, sizeof(struct vm_block), aos_slab_refill);
    slab_grow(&st->slabs, st->virtual_memory_regions,sizeof(st->virtual_memory_regions));

    struct vm_block* initial_free_space=slab_alloc(&st->slabs);
    initial_free_space->type=VirtualBlock_Free;
    initial_free_space->prev=NULL;
    initial_free_space->next=NULL;
    initial_free_space->start_address=start_vaddr;
    initial_free_space->size=VADDR_OFFSET;    //TODO: Figure out how to limit size of virtual memory

    st->head=initial_free_space;
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
    struct capref pdir;    //TODO: Check what this paramether is for
    paging_init_state(&current, VADDR_OFFSET, pdir, get_default_slot_allocator());
    set_current_paging_state(&current);

    // TODO: maybe add paging regions to paging state?

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
    // TIP: you will need to keep track of possible holes in the region
    return SYS_ERR_OK;
}

/**
 *
 * \brief Find a bit of free virtual address space that is large enough to
 *        accomodate a buffer of size `bytes`.
 */
errval_t paging_alloc(struct paging_state *st, void **buf, size_t bytes)
{
    debug_printf("paging_alloc: invoked!\n");

    // TODO: M2 Implement this function
    struct vm_block* virtual_addr=st->head;
    for(;virtual_addr!=NULL;virtual_addr=virtual_addr->next){
        //If it is used, just skip it
        if(virtual_addr->type==VirtualBlock_Allocated)
            continue;

        //if it is exact same size, just retype it
        if(virtual_addr->size==bytes){
            debug_printf("Retyping block!\n");
            virtual_addr->type=VirtualBlock_Allocated;
            *buf=(void*)virtual_addr->start_address;
            return SYS_ERR_OK;
        }else if(virtual_addr->size>bytes){
            debug_printf("Spliting block for %lu bytes!\n", bytes);

            if (!slab_has_freecount(&st->slabs, 2)){
                debug_printf("Slab count is less than 2, refilling\n");
                st->slabs.refill_func(&st->slabs);
            }

            struct vm_block* used_space=slab_alloc(&st->slabs);
            debug_printf("Received block: 0x%X!\n",used_space);
            used_space->type=VirtualBlock_Allocated;
            debug_printf("Written to newly allocated block!\n",bytes);
            used_space->prev=virtual_addr->prev;
            if(used_space->prev!=NULL){
                used_space->prev->next=used_space;
            }
            virtual_addr->prev=used_space;
            used_space->next=virtual_addr;
            used_space->size=bytes;
            virtual_addr->size-=bytes;
            used_space->start_address=virtual_addr->start_address;
            virtual_addr->start_address+=bytes;
            *buf=(void*)used_space->start_address;
            debug_printf("Finished creating new block!\n");
            return SYS_ERR_OK;
        }else{
            continue;
        }
    }
    return SYS_ERR_OK;    //TODO: Throw adequate error
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
    err=paging_map_fixed_attr(st, (lvaddr_t)(*buf), frame, bytes, flags);
    debug_printf("paging_map_frame_attr: out\n");
    return err;
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
    if (!bytes)
        return PAGE_ERR_NO_BYTES;

    debug_printf("mapping address: 0x%X of size: %lu\n",vaddr,bytes);

    capaddr_t l1_slot = ARM_L1_OFFSET(vaddr);
    capaddr_t l1_slot_end = ARM_L1_OFFSET(vaddr + bytes - 1);
    capaddr_t l2_slot = ARM_L2_OFFSET(vaddr);
    if (l1_slot != l1_slot_end)
    {
        debug_printf("Several l2 pages\n",vaddr,bytes);
        for (; l1_slot < l1_slot_end; ++l1_slot)
        {
            size_t bytes_this_l1 = LARGE_PAGE_SIZE -
                (ARM_L2_OFFSET(vaddr) << BASE_PAGE_BITS);
            errval_t err = paging_map_fixed_attr(st, vaddr, frame, bytes_this_l1, flags);
            if (err_is_fail(err))
                return err;
            vaddr += bytes_this_l1;
            bytes -= bytes_this_l1;
        }
        return paging_map_fixed_attr(st, vaddr, frame, bytes, flags);
    }
    debug_printf("Single l2 page table\n",vaddr,bytes);
    // TODO:
    // Copy if partially mapping large frames (cf Milestone1.pdf)
    // cap_copy(struct capref dest, struct capref src)
    // 1. Find L1 table
    struct capref l1_pagetable = {
        .cnode = cnode_page,
        .slot = 0,
    };
    errval_t err;

    // 2. Find cap to L2 for mapping (possibly create it)
    struct capref* l2_cap=NULL;
    if(!st->l2nodes[l1_slot].used){
        err = arml2_alloc(st, &st->l2nodes[l1_slot].vnode_ref);
        if (err_is_fail(err))
            return err_push(err, PAGE_ERR_ALLOC_ARML2);

        st->l2nodes[l1_slot].used=true;

        struct capref mapping_l2_to_l1;
        err = st->slot_alloc->alloc(st->slot_alloc, &mapping_l2_to_l1);
        if (err_is_fail(err))
            return err_push(err, PAGE_ERR_ALLOC_SLOT);

        err = vnode_map(l1_pagetable, st->l2nodes[l1_slot].vnode_ref,
                l1_slot, VREGION_FLAGS_READ_WRITE,
                0, 1, mapping_l2_to_l1);
        if (err_is_fail(err))
            return err_push(err, PAGE_ERR_VNODE_MAP_L2);
    }

    l2_cap=&st->l2nodes[l1_slot].vnode_ref;
    MM_ASSERT(l2_cap!=NULL, "L2 capability should point to something");

    // 3. Map Frame to L2
    // TODO: Handle bad aligned vaddr?
    struct capref mapping_frame_to_l2;
    err = st->slot_alloc->alloc(st->slot_alloc, &mapping_frame_to_l2);
    if (err_is_fail(err))
        return err_push(err, PAGE_ERR_ALLOC_SLOT);

    err = vnode_map(*l2_cap, frame,
            l2_slot, flags,
            0, (((bytes- 1) / BASE_PAGE_SIZE) + 1), mapping_frame_to_l2);
    if (err_is_fail(err))
        return err_push(err, PAGE_ERR_VNODE_MAP_FRAME);
    debug_printf("paging_map_fixed_attr: out\n");
    return SYS_ERR_OK;
}

/**
 * \brief unmap a user provided frame, and return the VA of the mapped
 *        frame in `buf`.
 * NOTE: Implementing this function is optional.
 */
errval_t paging_unmap(struct paging_state *st, const void *region)
{
    lvaddr_t address_to_free=(lvaddr_t)region;
    struct vm_block* virtual_addr=st->head;
    for(;virtual_addr!=NULL;virtual_addr=virtual_addr->next){
        if(virtual_addr->start_address==address_to_free){

        }
    }

    return SYS_ERR_OK;
}
