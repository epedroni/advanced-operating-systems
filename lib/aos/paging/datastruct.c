#include <aos/aos.h>
#include <aos/paging.h>
#include <aos/except.h>
#include <aos/slab.h>
#include "threads_priv.h"
#include <mm/mm.h>

#include <stdio.h>
#include <string.h>



/**
 * \brief Finds the block at given address, or just before
 */
struct vm_block* find_block_before(struct paging_state *st, lvaddr_t before_address)
{
    struct vm_block* va = st->head;
    if (!va)
        return NULL;

    for(;va->next && va->next->start_address < before_address; va = va->next);
    return va;
}
