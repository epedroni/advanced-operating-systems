#include <aos/aos.h>
#include <aos/paging.h>
#include <aos/except.h>
#include <aos/slab.h>
#include "threads_priv.h"
#include <mm/mm.h>

#include <stdio.h>
#include <string.h>

bool is_block_valid(struct vm_block* b)
{
    #define CHECK_ERR(cond) { if (!(cond)) { debug_printf("Invalid block: \"" #cond "\" not true!\n"); return false;} }
    assert(b);
    CHECK_ERR(!b->next || b->next->prev == b);
    CHECK_ERR(!b->prev || b->prev->next == b);
    return true;
}

/**
 * \brief Finds a free block with given size
 */
struct vm_block* find_free_block_with_size(struct paging_state *st, size_t min_size, vm_block_key_t* key)
{
    for (struct vm_block* va = st->head;va; va = va->next)
    {
        if (va->type != VirtualBlock_Free||
            va->size < min_size)
            continue;
        *key = va;
        assert(is_block_valid(va));
        return va;
    }
    return NULL;
}

/**
 * \brief Finds the last block before given address
 */
struct vm_block* find_block_before(struct paging_state *st, lvaddr_t before_address, vm_block_key_t* key)
{
    struct vm_block* va = st->head;
    if (!va)
        return NULL;

    for(;va->next && va->next->start_address <= before_address; va = va->next);
    assert(is_block_valid(va));
    *key = va;
    return va;
}

struct vm_block* add_block_after(struct paging_state* st,
    vm_block_key_t original, lvaddr_t at_address, vm_block_key_t* new_key)
{
    struct vm_block* new_block = slab_alloc(&st->slabs);
    assert(is_block_valid(original));
    new_block->next = original->next;
    new_block->prev = original;
    if (new_block->next)
        new_block->next->prev = new_block;
    original->next = new_block;
    assert(is_block_valid(original));
    assert(is_block_valid(new_block));

    new_block->start_address = at_address;
    *new_key = new_block;
    return new_block;
}

/**
 * \brief Merges next node to current node, and update list links accordingly.
 *          The next node is deleted with the slab allocator.
 */
void vm_block_merge_next_into_me(struct paging_state *st, struct vm_block* virtual_addr)
{
    assert(virtual_addr->next);
    struct vm_block* node_to_free = virtual_addr->next;
    // Linked list
    virtual_addr->next = virtual_addr->next->next;
    if (virtual_addr->next)
        virtual_addr->next->prev = virtual_addr;
    // Free
    slab_free(&st->slabs, node_to_free);
    assert(is_block_valid(virtual_addr));
}

struct vm_block* create_root(struct paging_state* st, size_t start_address)
{
    struct vm_block* new_block = slab_alloc(&st->slabs);
    new_block->next = NULL;
    new_block->prev = NULL;
    new_block->start_address = start_address;
    st->head = new_block;
    return new_block;
}
