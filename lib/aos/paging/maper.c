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

errval_t paging_refill_own_allocator(struct paging_state *state);

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
    st->l1_pagetable = pdir;
    st->slot_alloc = ca;
    st->cap_slot_in_own_space = NULL_CAP;

    memset(st->l2nodes, 0, sizeof(st->l2nodes));
    slab_init(&st->slabs, sizeof(struct vm_block), aos_slab_refill);
    slab_grow(&st->slabs, st->virtual_memory_regions, sizeof(st->virtual_memory_regions));

    struct vm_block* initial_free_space=create_root(st);
    initial_free_space->type=VirtualBlock_Free;
    initial_free_space->start_address=start_vaddr;
    initial_free_space->size=VADDR_OFFSET;    //TODO: Figure out how to limit size of virtual memory

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
    DEBUG_PAGING("Initializing paging@0x%08x... \n", (int)&current);

    struct capref l1_pagetable = {
        .cnode = cnode_page,
        .slot = 0,
    };
    paging_init_state(&current, VADDR_OFFSET, l1_pagetable, get_default_slot_allocator());
    set_current_paging_state(&current);
    paging_init_onthread(NULL); // Sets exception handler

    // TODO: maybe add paging regions to paging state?
    return SYS_ERR_OK;
}

/**
 * \brief return a pointer to a bit of the paging region `pr`.
 * This function gets used in some of the code that is responsible
 * for allocating Frame (and other) capabilities.
 */
errval_t paging_region_init(struct paging_state *st, struct paging_region *pr, size_t size)
{
    void *base;
    errval_t err = paging_alloc(st, &base, size, NULL);
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
errval_t paging_alloc(struct paging_state *st, void **buf, size_t bytes, struct vm_block** block)
{
    #ifdef PAGING_KEEP_GAPS
        bytes += PAGING_KEEP_GAPS * BASE_PAGE_SIZE;
    #endif

    bytes = ROUND_UP(bytes, BASE_PAGE_SIZE);
    struct vm_block* virtual_addr=st->head;
    for(;virtual_addr!=NULL;virtual_addr=virtual_addr->next){
        // If it is used or too small, skip it
        if (virtual_addr->type == VirtualBlock_Allocated ||
            virtual_addr->size < bytes){
            continue;
        }

        // Mark this one as used - for calls to alloc from refill functions
        // indirectly called here.
        virtual_addr->type = VirtualBlock_Allocated;
        virtual_addr->map_flags = 0;

        //if it is exact same size, just retype it
        if (virtual_addr->size==bytes){
            *buf=(void*)virtual_addr->start_address;
            assert(is_block_valid(virtual_addr));
            return SYS_ERR_OK;
        }
        assert(virtual_addr->size > bytes);

        struct vm_block* remaining_free_space = add_block_after(st, virtual_addr);
        assert(remaining_free_space);
        // Create block for remaining free size
        remaining_free_space->type = VirtualBlock_Free;
        remaining_free_space->start_address = virtual_addr->start_address + bytes;
        remaining_free_space->size = virtual_addr->size - bytes;
        virtual_addr->size = bytes;
        *buf=(void*)virtual_addr->start_address;

        assert(is_block_valid(virtual_addr));
        assert(is_block_valid(remaining_free_space));

        if (block)
            *block = virtual_addr;

        if (!slab_has_freecount(&st->slabs, 5))
            paging_refill_own_allocator(st);
        return SYS_ERR_OK;
    }
    return PAGE_ERR_OUT_OF_VMEM;
}

errval_t paging_alloc_fixed_address(struct paging_state *st, lvaddr_t desired_address, size_t bytes)
{
    lvaddr_t start_address = ROUND_DOWN((lvaddr_t) desired_address, BASE_PAGE_SIZE);

    struct vm_block* virtual_addr = find_block_before(st, start_address);
    if (!virtual_addr ||
        virtual_addr->start_address + virtual_addr->size < desired_address + bytes ||
        virtual_addr->type != VirtualBlock_Free)
        return PAGE_ERR_OUT_OF_VMEM;

    //if we have some space in the beginning, split it
    if(virtual_addr->start_address<start_address){
        debug_printf("Splitting front part\n");
        struct vm_block* previous_block=virtual_addr;
        size_t prev_size=previous_block->size;
        previous_block->size=start_address-previous_block->size;

        virtual_addr = add_block_after(st, previous_block);
        virtual_addr->type=VirtualBlock_Free;
        virtual_addr->start_address=start_address;
        virtual_addr->size=prev_size-previous_block->size;
    }

    virtual_addr->type = VirtualBlock_Allocated;
    virtual_addr->map_flags = 0;
    if (virtual_addr->size==bytes){
        debug_printf("we have super alligned block, returning\n");
        return SYS_ERR_OK;
    }

    assert(virtual_addr->size > bytes);
    struct vm_block* remaining_free_space = add_block_after(st, virtual_addr);
    // Create block for remaining free size
    remaining_free_space->type = VirtualBlock_Free;
    remaining_free_space->start_address = virtual_addr->start_address + bytes;
    remaining_free_space->size = virtual_addr->size - bytes;
    virtual_addr->size = bytes;

    if (!slab_has_freecount(&st->slabs, 5))
        paging_refill_own_allocator(st);
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
    struct vm_block* block;
    ERROR_RET1(paging_alloc(st, buf, bytes, &block));
    assert(block);
    return paging_map_fixed_attr(st, (lvaddr_t)(*buf), frame, bytes, 0, flags, block);
}

errval_t
paging_refill_own_allocator(struct paging_state *st)
{
    if (st->is_refilling_slab)
        return SYS_ERR_OK;

    // We should always have a block ready for slab next refill
    st->is_refilling_slab = true;
    char* buf = malloc(BASE_PAGE_SIZE);
    buf[0] = 0; // Touch it, so we don't pagefault
    slab_grow(&st->slabs, buf, BASE_PAGE_SIZE);
    st->is_refilling_slab = false;
    return SYS_ERR_OK;
}

/**
 * \brief map a user provided frame at user provided VA.
 */
errval_t paging_map_fixed_attr(struct paging_state *st, lvaddr_t vaddr,
        struct capref frame, size_t bytes, size_t offset, int flags, struct vm_block* block)
{
    DEBUG_PAGING("Paging: 0x%08x .. + 0x%08x [offset 0x%08x]\n",
        (int)vaddr, (int)bytes, (int)offset);

    if (!bytes)
        return PAGE_ERR_NO_BYTES;
    if (BASE_PAGE_OFFSET(vaddr))
        return PAGE_ERR_VADDR_NOT_ALIGNED;
    if (offset != 0 && BASE_PAGE_OFFSET(offset))
        return PAGE_ERR_OFFSET_NOT_ALIGNED;

    capaddr_t l1_slot = ARM_L1_OFFSET(vaddr);
    capaddr_t l1_slot_end = ARM_L1_OFFSET(vaddr + bytes - 1);
    capaddr_t l2_slot = ARM_L2_OFFSET(vaddr);
    if (l1_slot != l1_slot_end)
    {
        DEBUG_PAGING("Several L2 map [%u - %u]\n",
            (int)l1_slot, (int)l1_slot_end);
        for (; l1_slot < l1_slot_end; ++l1_slot)
        {
            size_t bytes_this_l1 = LARGE_PAGE_SIZE -
                (ARM_L2_OFFSET(vaddr) << BASE_PAGE_BITS);
            ERROR_RET1(paging_map_fixed_attr(st, vaddr,
                frame, bytes_this_l1, offset,
                flags, block));
            vaddr += bytes_this_l1;
            offset += bytes_this_l1;
            bytes -= bytes_this_l1;
        }
        ERROR_RET1(paging_map_fixed_attr(st, vaddr, frame, bytes, offset, flags, block));
        DEBUG_PAGING("Several L2 map finished\n");
        return SYS_ERR_OK;
    }

    // 1. Find cap to L2 for mapping (possibly create it)
    struct capref l2_cap;
    if(!st->l2nodes[l1_slot].used){
        ERROR_RET2(arml2_alloc(st, &st->l2nodes[l1_slot].vnode_ref),
            PAGE_ERR_ALLOC_ARML2);

        st->l2nodes[l1_slot].used=true;

        struct capref mapping_l2_to_l1;
        ERROR_RET2(st->slot_alloc->alloc(st->slot_alloc, &mapping_l2_to_l1),
            PAGE_ERR_ALLOC_SLOT);

        ERROR_RET2(vnode_map(st->l1_pagetable, st->l2nodes[l1_slot].vnode_ref,
                l1_slot, VREGION_FLAGS_READ_WRITE,
                0, 1, mapping_l2_to_l1),
                PAGE_ERR_VNODE_MAP_L2);
    }

    l2_cap = st->l2nodes[l1_slot].vnode_ref;

    // 2. Map Frame to L2
    struct capref mapping_ref;
    ERROR_RET2(st->slot_alloc->alloc(st->slot_alloc, &mapping_ref),
        PAGE_ERR_ALLOC_SLOT);

    bool remote = get_croot_addr(l2_cap) != CPTR_ROOTCN;
    if (remote)
    {
        struct capref l2_cap_remote = l2_cap;
        ERROR_RET1(get_default_slot_allocator()->alloc(
            get_default_slot_allocator(),
            &l2_cap));
        ERROR_RET1(cap_copy(l2_cap, l2_cap_remote));
    }
    ERROR_RET2(vnode_map(l2_cap, frame,
            l2_slot, flags,
            offset, (((bytes- 1) / BASE_PAGE_SIZE) + 1), mapping_ref),
            PAGE_ERR_VNODE_MAP_FRAME);
    if (remote)
        ERROR_RET1(cap_destroy(l2_cap));

    if (block)
        block->mapping = mapping_ref;
    return SYS_ERR_OK;
}

/**
 * \brief unmap a user provided frame, and return the VA of the mapped
 *        frame in `buf`.
 */
errval_t paging_unmap(struct paging_state *st, const void *region)
{
    lvaddr_t address_to_free=(lvaddr_t)region;
    struct vm_block* virtual_addr = find_block_before(st, address_to_free);
    if (!virtual_addr || virtual_addr->start_address != address_to_free)
        return PAGE_ERR_NOT_MAPPED;

    // TODO: Need to store several mappings in that case.
    // -> Linked list ie another slab etc...
    assert(ARM_L1_OFFSET(address_to_free) == ARM_L1_OFFSET(address_to_free + virtual_addr->size - 1) &&
        "Unmap for mappings spawning over different L2 VNodes not implemented yet.");
    ERROR_RET1(vnode_unmap(st->l2nodes[ARM_L1_OFFSET(address_to_free)].vnode_ref,
        virtual_addr->mapping));
    // Merge with previous <- me
    if (virtual_addr->prev && virtual_addr->prev->type == VirtualBlock_Free)
    {
        virtual_addr->prev->size += virtual_addr->size;
        virtual_addr->mapping = virtual_addr->mapping;
        virtual_addr = virtual_addr->prev;
        vm_block_merge_next_into_me(st, virtual_addr);
    }
    // Merge with me <- next
    if (virtual_addr->next && virtual_addr->next->type == VirtualBlock_Free)
    {
        virtual_addr->size += virtual_addr->next->size;
        vm_block_merge_next_into_me(st, virtual_addr);
    }
    virtual_addr->type = VirtualBlock_Free;
    return SYS_ERR_OK;
}
