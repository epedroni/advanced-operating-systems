/**
 * \file
 * \brief create child process library
 */

/*
 * Copyright (c) 2016, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetsstrasse 6, CH-8092 Zurich. Attn: Systems Group.
 */

#ifndef _INIT_SPAWN_H_
#define _INIT_SPAWN_H_

#include "aos/slot_alloc.h"
#include "aos/paging.h"

struct spawninfo {

    // Information about the binary
    char * binary_name;     // Name of the binary
    struct capref module_frame;
    size_t module_bytes;

    // TODO: Use this structure to keep track
    // of information you need for building/starting
    // your new process!
    struct cnoderef l2_cnodes[ROOTCN_SLOTS_USER];
    struct capref l1_cnode_cap;
    struct capref l1_pagetable_child_cap;
    struct capref l1_pagetable_own_cap;

    // Dispatcher
    struct capref child_dispatcher_own_cap;
    struct capref child_dispatcher_frame_own_cap;
    lvaddr_t dispatcher_frame_mapped_child;
    dispatcher_handle_t dispatcher_handle; // Dispatcher frame mapped for me

    lvaddr_t got;
    genvaddr_t child_entry_point;

    //Arguments
    struct capref child_arguments_frame_own_cap;

    // Child paging information
    struct capref child_l2_pt_own_cap;
    int pagecn_next_slot;
    struct paging_state child_paging_state;
};

// Start a child process by binary name. Fills in si
errval_t spawn_load_by_name(void * binary_name, struct spawninfo * si);

#endif /* _INIT_SPAWN_H_ */
