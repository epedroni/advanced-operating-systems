/**
 * \file
 * \brief Barrelfish paging helpers.
 */

/*
 * Copyright (c) 2012, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Haldeneggsteig 4, CH-8092 Zurich. Attn: Systems Group.
 */


#ifndef LIBBARRELFISH_PAGING_H
#define LIBBARRELFISH_PAGING_H

#include <errors/errno.h>
#include <aos/capabilities.h>
#include <aos/slab.h>
#include <aos/paging_datastruct.h>
#include <barrelfish_kpi/paging_arm_v7.h>

typedef int paging_flags_t;

#define VADDR_OFFSET ((lvaddr_t)1UL*1024*1024*1024) // 1GB

#define PAGING_SLAB_BUFSIZE 12

#define VREGION_FLAGS_READ     0x01 // Reading allowed
#define VREGION_FLAGS_WRITE    0x02 // Writing allowed
#define VREGION_FLAGS_EXECUTE  0x04 // Execute allowed
#define VREGION_FLAGS_NOCACHE  0x08 // Caching disabled
#define VREGION_FLAGS_MPB      0x10 // Message passing buffer
#define VREGION_FLAGS_GUARD    0x20 // Guard page
#define VREGION_FLAGS_MASK     0x2f // Mask of all individual VREGION_FLAGS

#define VREGION_FLAGS_READ_WRITE \
    (VREGION_FLAGS_READ | VREGION_FLAGS_WRITE)
#define VREGION_FLAGS_READ_EXECUTE \
    (VREGION_FLAGS_READ | VREGION_FLAGS_EXECUTE)
#define VREGION_FLAGS_READ_WRITE_NOCACHE \
    (VREGION_FLAGS_READ | VREGION_FLAGS_WRITE | VREGION_FLAGS_NOCACHE)
#define VREGION_FLAGS_READ_WRITE_MPB \
    (VREGION_FLAGS_READ | VREGION_FLAGS_WRITE | VREGION_FLAGS_MPB)

// struct to store the paging status of a process
struct l2_vnode_ref {
    bool used;
    struct capref vnode_ref;
};

//#define PAGING_KEEP_GAPS 40
#define DEBUG_PAGING(s, ...) //debug_printf("[PAGING] " s, ##__VA_ARGS__)


struct paging_state {
    struct slot_allocator* slot_alloc;
    struct l2_vnode_ref l2nodes[ARM_L1_MAX_ENTRIES];
    struct vm_block slab_init_buffer[15];    //Lets give some buffer for slab to allocate
    struct slab_allocator slabs;    //slab allocator used for allocating vm_blocks
    vm_block_struct_t blocks;
    struct capref l1_pagetable;

    struct capref cap_slot_in_own_space;

    bool is_refilling_slab;

    struct thread_mutex page_fault_lock;
    struct thread_mutex blocks_lock;
};

#define DATA_STRUCT_LOCK(st) { thread_mutex_lock(&(st)->blocks_lock);}
#define DATA_STRUCT_UNLOCK(st) { thread_mutex_unlock(&(st)->blocks_lock);}

extern errval_t aos_slab_refill(struct slab_allocator *slabs);

struct thread;
/// Initialize paging_state struct
errval_t paging_init_state(struct paging_state *st, lvaddr_t start_vaddr,
        struct capref pdir, struct slot_allocator * ca);
/// initialize self-paging module
errval_t paging_init(void);
/// setup paging on new thread (used for user-level threads)
void paging_init_onthread(struct thread *t);

errval_t paging_region_init(struct paging_state *st,
                            struct paging_region *pr, size_t size);

/**
 * \brief return a pointer to a bit of the paging region `pr`.
 * This function gets used in some of the code that is responsible
 * for allocating Frame (and other) capabilities.
 */
errval_t paging_region_map(struct paging_region *pr, size_t req_size,
                           void **retbuf, size_t *ret_size);
/**
 * \brief free a bit of the paging region `pr`.
 * This function gets used in some of the code that is responsible
 * for allocating Frame (and other) capabilities.
 * We ignore unmap requests right now.
 */
errval_t paging_region_unmap(struct paging_region *pr, lvaddr_t base, size_t bytes);

/**
 * \brief Find a bit of free virtual address space that is large enough to
 *        accomodate a buffer of size `bytes`.
 */
errval_t paging_alloc(struct paging_state *st, void **buf, size_t bytes, struct vm_block** block);
errval_t paging_alloc_fixed_address(struct paging_state *st, lvaddr_t desired_address, size_t bytes);
errval_t paging_mark_as_paged_address(struct paging_state *st, lvaddr_t desired_address, size_t bytes);

/**
 * Functions to map a user provided frame.
 */
/// Map user provided frame with given flags while allocating VA space for it
errval_t paging_map_frame_attr(struct paging_state *st, void **buf,
                               size_t bytes, struct capref frame,
                               int flags, void *arg1, void *arg2);
/// Map user provided frame at user provided VA with given flags.
errval_t paging_map_fixed_attr(struct paging_state *st, lvaddr_t vaddr,
                               struct capref frame, size_t bytes, size_t offset,
                               int flags, struct vm_block* block);

/**
 * refill slab allocator without causing a page fault
 */
errval_t slab_refill_no_pagefault(struct slab_allocator *slabs, struct capref frame, size_t minbytes);

/**
 * \brief unmap region starting at address `region`.
 * NOTE: this function is currently here to make libbarrelfish compile. As
 * noted on paging_region_unmap we ignore unmap requests right now.
 */
errval_t paging_unmap(struct paging_state *st, const void *region);


/// Map user provided frame while allocating VA space for it
static inline errval_t paging_map_frame(struct paging_state *st, void **buf,
                                        size_t bytes, struct capref frame,
                                        void *arg1, void *arg2)
{
    return paging_map_frame_attr(st, buf, bytes, frame,
            VREGION_FLAGS_READ_WRITE, arg1, arg2);
}

/// Map user provided frame at user provided VA.
static inline errval_t paging_map_fixed(struct paging_state *st, lvaddr_t vaddr,
                                        struct capref frame, size_t bytes)
{
    return paging_map_fixed_attr(st, vaddr, frame, bytes, 0,
            VREGION_FLAGS_READ_WRITE, NULL);
}

static inline lvaddr_t paging_genvaddr_to_lvaddr(genvaddr_t genvaddr) {
    return (lvaddr_t) genvaddr;
}


#endif // LIBBARRELFISH_PAGING_H
