/**
 * \file
 * \brief A library for managing physical memory (i.e., caps)
 */

#include <mm/mm.h>
#include <aos/debug.h>
#include <aos/paging.h>

/// MM allocator instance data
struct mm aos_mm;
extern struct bootinfo *bi;

errval_t mm_alloc_cap(struct mm* mm, struct capref* ref);

/**
 * Initialize the memory manager.
 *
 * \param  mm               The mm struct to initialize.
 * \param  objtype          The cap type this manager should deal with.
 * \param  slab_refill_func Custom function for refilling the slab allocator.
 * \param  slot_alloc_func  Function for allocating capability slots.
 * \param  slot_refill_func Function for refilling (making) new slots.
 * \param  slot_alloc_inst  Pointer to a slot allocator instance (typically passed to the alloc and refill functions).
 */
errval_t mm_init(struct mm *mm, enum objtype objtype,
                 slab_refill_func_t slab_refill_func,
                 slot_alloc_t slot_alloc_func,
                 slot_refill_t slot_refill_func,
                 void *slot_alloc_inst)
{
    assert(mm != NULL);
    mm->slot_alloc = slot_alloc_func;
    mm->slot_refill = slot_refill_func;
    mm->slot_alloc_inst = slot_alloc_inst;
    mm->objtype = objtype;
    mm->head = NULL;

    slab_init(&(mm->slabs), sizeof(struct mmnode), slab_refill_func);
    SLAB_SET_NAME(&mm->slabs, "mm");
    thread_mutex_init(&mm->nodes_lock);
    return SYS_ERR_OK;
}

/**
 * Destroys the memory allocator.
 */
void mm_destroy(struct mm *mm)
{
}

/**
 * Adds a capability to the memory manager.
 *
 * \param  cap  Capability to add
 * \param  base Physical base address of the capability
 * \param  size Size of the capability (in bytes)
 */
errval_t mm_add(struct mm *mm, struct capref cap, genpaddr_t base, size_t size)
{
    struct mmnode* newnode = slab_alloc(&mm->slabs);
    newnode->type = NodeType_Free;
    newnode->cap.cap = cap;
    newnode->cap.base = base;
    newnode->cap.size = size;
    newnode->prev = NULL;
    newnode->base = base;
    newnode->size = size;
    LIBMM_STRUCT_LOCK(mm);
    newnode->next = mm->head;
    if (mm->head)
        mm->head->prev = newnode;
    mm->head = newnode;
    LIBMM_STRUCT_UNLOCK(mm);

    debug_printf("mm_add received %lu MB\n", size / 1024 / 1024);

    return SYS_ERR_OK;
}

/**
 * Splits a memory node. $node is shrinked to $size, and
 * a second node is insert after $node.
 *
 * \param       mm        The memory manager.
 * \param       node      Node to split.
 * \param       size      Size of the first chunk.
 */
inline void mm_split_mem_node_unsafe(struct mm* mm, struct mmnode* node, size_t size)
{
    // Split this node in 2 nodes
    struct mmnode* remaining_free = slab_alloc(&mm->slabs);
    remaining_free->type = NodeType_Free;
    remaining_free->cap = node->cap;
    remaining_free->base = node->base + size;
    remaining_free->size = node->size - size;

    // Shrink initial node
    node->size = size;

    // Link the list
    remaining_free->prev = node;
    remaining_free->next = node->next;
    if (node->next)
        node->next->prev = remaining_free;
    node->next = remaining_free;
}

errval_t mm_alloc_cap(struct mm* mm, struct capref* ref)
{
    // Note: mm_alloc_cap_refill will call mm_alloc,
    // but this is handled in 'slot_refill' correctly, don't worry here.
    // Note2: This function only refills when needed.
    errval_t err = mm->slot_refill(mm->slot_alloc_inst);
    if (err_is_fail(err)) {
        return err_push(err, MM_ERR_SLOT_ALLOC_REFILL);
    }

    err = mm->slot_alloc(mm->slot_alloc_inst, 1, ref);
    if (err_is_fail(err)) {
        return err_push(err, MM_ERR_SLOT_ALLOC_REFILL);
    }
    return SYS_ERR_OK;
}

/**
 * Allocate aligned physical memory.
 *
 * \param       mm        The memory manager.
 * \param       size      How much memory to allocate.
 * \param       alignment The alignment requirement of the base address for your memory.
 * \param[out]  retcap    Capability for the allocated region.
 */
errval_t mm_alloc_aligned(struct mm *mm, size_t size, size_t alignment, struct capref *retcap)
{
    if (alignment % BASE_PAGE_SIZE)
        return LIB_ERR_RAM_ALLOC_WRONG_SIZE;

    size_t aligned_size = ((size-1) / BASE_PAGE_SIZE + 1) * BASE_PAGE_SIZE;

    LIBMM_STRUCT_LOCK(mm);

    // See comment on maper.c for explanation about the magic 6.
    if (!slab_has_freecount(&mm->slabs, 6*3+2))
        mm->slabs.refill_func(&mm->slabs);


    struct mmnode* node = mm->head;
    if (!node)
    {
        LIBMM_STRUCT_UNLOCK(mm);
        return MM_ERR_NO_NODE;
    }
    while (node != NULL)
    {
        if (node->type == NodeType_Free)
        {
            size_t align_pad = (alignment - node->base % alignment) % alignment;
            if (node->size >= (aligned_size + align_pad))
            {
                genpaddr_t base = node->base + align_pad;
                // 1. Split to align
                if (align_pad)
                {
                    mm_split_mem_node_unsafe(mm, node, align_pad);
                    node = node->next;
                    assert(node);
                }
                // 2. split the mem we need
                if (node->size > size){
                    mm_split_mem_node_unsafe(mm, node, aligned_size);
                }

                // 3. Alloc cap for returned value
                errval_t err = mm_alloc_cap(mm, retcap);
                if (err_is_fail(err))
                {
                    LIBMM_STRUCT_UNLOCK(mm);
                    return err;
                }

                err = cap_retype(*retcap, node->cap.cap, base - node->cap.base,
                        mm->objtype, size, 1);
                if (err_is_fail(err))
                {
                    LIBMM_STRUCT_UNLOCK(mm);
                    return err;
                }

                node->type = NodeType_Allocated;
                LIBMM_STRUCT_UNLOCK(mm);
                return err;
            }
        }
        node = node->next;
    }
    LIBMM_STRUCT_UNLOCK(mm);
    return MM_ERR_OUT_OF_MEMORY;
}

/**
 * Allocate physical memory.
 *
 * \param       mm        The memory manager.
 * \param       size      How much memory to allocate.
 * \param[out]  retcap    Capability for the allocated region.
 */
errval_t mm_alloc(struct mm *mm, size_t size, struct capref *retcap)
{
    return mm_alloc_aligned(mm, size, BASE_PAGE_SIZE, retcap);
}

/**
 * Merges a given memory node with the following one.
 * if both of them are of type 'NodeType_Free'.
 * $node_first and $node_first->next should be valid pointers.
 * $node_first remains valid, and $node_first->next is possibly deleted.
 *
 * \param       mm          The memory manager.
 * \param       node_first  Node to merge with $node->next.
 */
inline void mm_merge_mem_node_if_free_unsafe(struct mm* mm, struct mmnode* node_first)
{
    if (node_first->type != NodeType_Free || node_first->next->type != NodeType_Free)
        return;
    struct mmnode* node_next = node_first->next;

    node_first->size += node_first->next->size;
    node_first->next = node_next->next;
    if (node_first->next)
    	node_first->next->prev = node_first;

    slab_free(&mm->slabs, node_next);
}

void mm_print_nodes(struct mm* mm)
{
	struct mmnode* node = mm->head;
	int totalsize = 0;
	debug_printf("Head:\n");
	while (node) {
		debug_printf("Node for 0x%08x %10uB %s\n",
            (int)node->base,
            (int)node->size,
            node->type == NodeType_Allocated ? "ALLOCATED" : "FREE");
		if (node->type == NodeType_Free)
            totalsize += node->size;
		node = node->next;
	}
	debug_printf("Total free space: %u B\n", totalsize);
}

/**
 * Free a certain region (for later re-use).
 *
 * \param       mm        The memory manager.
 * \param       cap       The capability to free.
 * \param       base      The physical base address of the region.
 * \param       size      The size of the region.
 */
errval_t mm_free(struct mm *mm, struct capref cap, genpaddr_t base, gensize_t size)
{
    LIBMM_STRUCT_LOCK(mm);
    struct mmnode* node = mm->head;
    // Find node
    while (node != NULL && node->base != base)
        node = node->next;

    // This node does not exist!
    if (!node)
    {
        LIBMM_STRUCT_UNLOCK(mm);
        return MM_ERR_FIND_NODE;
    }
    if (node->type != NodeType_Allocated)
    {
        LIBMM_STRUCT_UNLOCK(mm);
        return MM_ERR_NOT_ALLOCATED;
    }

    // Merge with previous if the previous one is free
    // (We may need to merge with previous AND next node - see aligned alloc)
    node->type = NodeType_Free;
    if (node->next)
        mm_merge_mem_node_if_free_unsafe(mm, node);
    if (node->prev)
        mm_merge_mem_node_if_free_unsafe(mm, node->prev);
    LIBMM_STRUCT_UNLOCK(mm);
    //! $node may be invalid now!
    // Revoke the cap.
    //cap_revoke(cap) // NOT YET IMPLEMENTED!
    // Destroy the capability (and frees the slot)
    return cap_destroy(cap);
}
