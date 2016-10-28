#include <aos/aos.h>
#include <aos/paging.h>
#include <aos/except.h>
#include <aos/slab.h>
#include "threads_priv.h"
#include <mm/mm.h>

#include <stdio.h>
#include <string.h>


static void paging_thread_exception_handler(enum exception_type type, int val1, void* data, union registers_arm* registers, void ** whatever);


static errval_t handle_pagefault(void *addr)
{
    debug_printf("Pagefault at 0x%08x\n", addr);
    /*
    errval_t result = SYS_ERR_OK;
    errval_t err;
    genvaddr_t vaddr = vspace_lvaddr_to_genvaddr((lvaddr_t)addr);
    if (vaddr > VSPACE_BEGIN) {
        if (is_in_pmap(vaddr)) {
            printf("handle_pagefault: returning -- mapping exists already in pmap?\n");
        } else {
            printf("handle_pagefault: no mapping for address, allocating frame\n");
            struct capref frame;
            err = alloc_4k(&frame);
            if (err_is_fail(err)) {
                DEBUG_ERR(err, "alloc_4k");
            }
            struct pmap *pmap = get_current_pmap();
            err = pmap->f.map(pmap, vaddr, frame, 0, 4096,
                              VREGION_FLAGS_READ_WRITE, NULL, NULL);
            if (err_is_fail(err)) {
                DEBUG_ERR(err, "pmap->f.map");
            }
            printf("handle_pagefault: returning -- did install page\n");
            return SYS_ERR_OK;
        }
    } else {
        printf("handle_pagefault: invalid access to %p (< 0x%x)\n", addr, VSPACE_BEGIN);
        // TODO: good error code
        return LIB_ERR_PMAP_ADDR_NOT_FREE;
    }*/
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
            handle_pagefault(addr);
            break;
        default:
            debug_printf("[EXCN] Catched unknown exception type 0x%02x\n", type);
            break;
    }
}


/**
 * \brief Initialize per-thread paging state
 */
void paging_init_onthread(struct thread *t)
{
    thread_set_exception_handler(paging_thread_exception_handler,
          NULL,
          NULL, NULL,
          NULL, NULL);
}

errval_t slab_refill_no_pagefault(struct slab_allocator *slabs, struct capref frame, size_t minbytes)
{
    return SYS_ERR_NOT_IMPLEMENTED;
}
