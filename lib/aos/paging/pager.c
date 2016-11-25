#include <aos/aos.h>
#include <aos/paging.h>
#include <aos/except.h>
#include <aos/slab.h>
#include "threads_priv.h"
#include <mm/mm.h>

#include <stdio.h>
#include <string.h>

#define PF_DEBUG(...) //debug_printf(__VA_ARGS__);

static void paging_thread_exception_handler(enum exception_type type, int val1, void* data, union registers_arm* registers, void ** whatever);

static errval_t handle_pagefault(void *_addr)
{
    // TODO: MT environment: handle 2 pagefault at same addr, same time, diff threads
    // TODO: Handle unmap

    lvaddr_t addr = ROUND_DOWN((lvaddr_t)_addr, BASE_PAGE_SIZE);
    PF_DEBUG("[Pagefault@0x%08x] Entering callback\n", addr);
    struct paging_state* st = get_current_paging_state();

    thread_mutex_lock(&st->page_fault_lock);
    DATA_STRUCT_LOCK(st);

    // 1. Ensure we are on an allocated vmem block
    // ie block_base <= addr < block_base + block_size
    vm_block_key_t key;
    struct vm_block* block = find_block_before(st, addr, &key);
    if (!block || ADDRESS_FROM_VM_BLOCK_KEY(key) + block->size < addr){
        debug_printf("Address not in any block! This is SEGFAULT!\n");
        //print_backtrace();
        while(true);
        thread_mutex_unlock(&st->page_fault_lock);
        DATA_STRUCT_UNLOCK(st);
        return LIB_ERR_VSPACE_PAGEFAULT_ADDR_NOT_FOUND;
    }
    assert(ADDRESS_FROM_VM_BLOCK_KEY(key) <= addr);

    PF_DEBUG("Found block type: %d\n", block->type);

    if (block->type == VirtualBlock_Free){
        debug_printf("Address not allocated! This is SEGFAULT!\n");
        //print_backtrace();
        while(true);
        thread_mutex_unlock(&st->page_fault_lock);
        DATA_STRUCT_UNLOCK(st);
        return LIB_ERR_VSPACE_PAGEFAULT_ADDR_NOT_FOUND;
    }

    if (block->type == VirtualBlock_Paged){
        debug_printf("Address already paged, returning!\n");
        thread_mutex_unlock(&st->page_fault_lock);
        DATA_STRUCT_UNLOCK(st);
        return SYS_ERR_OK;
    }

    PF_DEBUG("[Pagefault@0x%08x] Block found @ 0x%08x [size 0x%08x]\n",
        addr, (int)ADDRESS_FROM_VM_BLOCK_KEY(key), block->size);

    // 2. It is the case: map a new frame here then
    struct capref frame;
    size_t actualsize;
    ERROR_RET2(frame_alloc(&frame, BASE_PAGE_SIZE, &actualsize),
        LIB_ERR_VSPACE_PAGEFAULT_HANDER);
    PF_DEBUG("[Pagefault@0x%08x] Frame allocated\n", addr);
    ERROR_RET2(paging_map_fixed_attr(st, addr,
        frame, BASE_PAGE_SIZE, 0,
        VREGION_FLAGS_READ_WRITE, NULL),
        LIB_ERR_VSPACE_PAGEFAULT_HANDER);

    paging_mark_as_paged_address(st, addr, BASE_PAGE_SIZE);

    PF_DEBUG("[Pagefault@0x%08x] Finished\n", addr);
    thread_mutex_unlock(&st->page_fault_lock);
    DATA_STRUCT_UNLOCK(st);
    return SYS_ERR_OK;
}

static void paging_thread_exception_handler(enum exception_type type,
    int subtype,
    void *addr, union registers_arm *regs,
    void **unused)
{
    switch (type)
    {
        case EXCEPT_PAGEFAULT:
        {
            errval_t err = handle_pagefault(addr);
            if (err_is_fail(err))
                DEBUG_ERR(err, "handle_pagefault");
            break;
        }
        default:
            debug_printf("[EXCN] Catched unknown exception type 0x%02x\n", type);
            break;
    }
}


#define INTERNAL_STACK_SIZE (1<<14)
static char internal_ex_stack[INTERNAL_STACK_SIZE];


/**
 * \brief Initialize per-thread paging state
 */
void paging_init_onthread(struct thread *t)
{
    char* ex_stack = internal_ex_stack;
    char* ex_stack_top = ex_stack + INTERNAL_STACK_SIZE;

    if (!t)
        t = thread_self();

    t->exception_handler = paging_thread_exception_handler;
    t->exception_stack = ex_stack;
    t->exception_stack_top = ex_stack_top;
}

/**
 * \brief Refills the slab without causing pagefault.
 * \param slabs: slab to refill
 * \param frame: valid slot to store a frame
 * \param minbytes: minimum size to provide
 */
errval_t slab_refill_no_pagefault(struct slab_allocator *slabs, struct capref frame, size_t minbytes)
{
    SLAB_DEBUG_OUT("[0x%08x:%s] Paging: slab_refill_no_pagefault",
        (int)slabs, slabs->name);
    minbytes = ROUND_UP(minbytes, BASE_PAGE_SIZE);
    struct capref ram_ref;
    void* data;
    ERROR_RET1(ram_alloc(&ram_ref, minbytes));
    ERROR_RET1(cap_retype(frame, ram_ref, 0, ObjType_Frame, minbytes, 1));
    ERROR_RET1(paging_map_frame(get_current_paging_state(),
        &data, minbytes, frame, NULL, NULL));

    slab_grow(slabs, data, minbytes);
    return SYS_ERR_OK;
}
