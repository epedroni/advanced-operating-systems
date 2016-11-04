#include <aos/aos.h>
#include <aos/paging.h>
#include <aos/except.h>
#include <aos/slab.h>
#include "threads_priv.h"
#include <mm/mm.h>

#include <stdio.h>
#include <string.h>


static void paging_thread_exception_handler(enum exception_type type, int val1, void* data, union registers_arm* registers, void ** whatever);

static errval_t handle_pagefault(void *_addr)
{
    // TODO: MT environment: handle 2 pagefault at same addr, same time, diff threads
    // TODO: Handle unmap

    lvaddr_t addr = ROUND_DOWN((lvaddr_t)_addr, BASE_PAGE_SIZE);
    debug_printf("[Pagefault@0x%08x] Entering callback\n", addr);
    struct paging_state* st = get_current_paging_state();

    thread_mutex_lock(&st->page_fault_lock);

    // 1. Ensure we are on an allocated vmem block
    // ie block_base <= addr < block_base + block_size
    vm_block_key_t key;
    struct vm_block* block = find_block_before(st, addr, &key);
    if (!block || address_from_vm_block_key(key) + block->size < addr){
        thread_mutex_unlock(&st->page_fault_lock);
        // This is a segfault.
        *((int*)NULL) = 0;
        return LIB_ERR_VSPACE_PAGEFAULT_ADDR_NOT_FOUND;
    }
    assert(address_from_vm_block_key(key) <= addr);
    debug_printf("Found block type: %d\n", block->type);

    if (block->type == VirtualBlock_Free){
        debug_printf("Address not allocated! This is SEGFAULT!\n");
        while(true);
        thread_mutex_unlock(&st->page_fault_lock);
        return LIB_ERR_VSPACE_PAGEFAULT_ADDR_NOT_FOUND;
    }

    if (block->type == VirtualBlock_Paged){
        debug_printf("Address already paged, returning!\n");
        thread_mutex_unlock(&st->page_fault_lock);
        return SYS_ERR_OK;
    }

    debug_printf("[Pagefault@0x%08x] Block found @ 0x%08x [size 0x%08x]\n",
        addr, (int)address_from_vm_block_key(key), block->size);
    // 2. It is the case: map a new frame here then
    struct capref frame;
    size_t actualsize;
    ERROR_RET2(frame_alloc(&frame, BASE_PAGE_SIZE, &actualsize),
        LIB_ERR_VSPACE_PAGEFAULT_HANDER);
    debug_printf("[Pagefault@0x%08x] Frame allocated\n", addr);
    ERROR_RET2(paging_map_fixed_attr(st, addr,
        frame, BASE_PAGE_SIZE, 0,
        VREGION_FLAGS_READ_WRITE, NULL),
        LIB_ERR_VSPACE_PAGEFAULT_HANDER);

    paging_mark_as_paged_address(st, addr, BASE_PAGE_SIZE);

    debug_printf("[Pagefault@0x%08x] Finished\n", addr);
    thread_mutex_unlock(&st->page_fault_lock);
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
    exception_handler_fn old_handler;
    void *old_stack_base, *old_stack_top;
    char* ex_stack = internal_ex_stack;
    char* ex_stack_top = ex_stack + INTERNAL_STACK_SIZE;

    errval_t err = thread_set_exception_handler(paging_thread_exception_handler,
          &old_handler,
          ex_stack, ex_stack_top,
          &old_stack_base, &old_stack_top);
    if (err_is_fail(err))
    {
        DEBUG_ERR(err, "thread_set_exception_handler");
        USER_PANIC("Cannot setup paging");
    }
}

/**
 * \brief Refills the slab without causing pagefault.
 * \param slabs: slab to refill
 * \param frame: valid slot to store a frame
 * \param minbytes: minimum size to provide
 */
errval_t slab_refill_no_pagefault(struct slab_allocator *slabs, struct capref frame, size_t minbytes)
{
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
