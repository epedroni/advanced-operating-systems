/**
 * \file
 * \brief A library for managing physical memory (i.e., caps)
 */

#include <mm/mm.h>
#include <aos/debug.h>


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
    newnode->prev = mm->head;
    newnode->next = NULL;
    if (mm->head)
        mm->head->next = newnode;
    newnode->base = base;
    newnode->size = size;
    mm->head = newnode;
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
void mm_split_mem_node(struct mm* mm, struct mmnode* node, size_t size);
void mm_split_mem_node(struct mm* mm, struct mmnode* node, size_t size)
{
    // Split this node in 2 nodes
    struct mmnode* remaining_free = slab_alloc(&mm->slabs);
    remaining_free->type = mm->objtype;
    remaining_free->cap = node->cap;
    remaining_free->base = node->base + size;
    remaining_free->size = node->size - size;

    // Shrink initial node
    node->size = size;

    // Link the list
    remaining_free->prev = node;
    remaining_free->next = node->next;
    node->next = remaining_free;
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
    // Find cap with enough space
    struct mmnode* node = mm->head;
    while (node != NULL)
    {
        if (node->type == NodeType_Free)
        {
            size_t align_pad = (alignment - node->base % alignment) % alignment;
            if (node->size >= (size + align_pad))
            {
                genpaddr_t base = node->base + align_pad;
                // 1. Split to align
                if (align_pad)
                {
                    mm_split_mem_node(mm, node, align_pad);
                    node = node->next;
                }
                // 2. split the mem we need
                if (node->size > size)
                    mm_split_mem_node(mm, node, size);
                // 3. Create cap for current node, and put it in retcap
                enum objtype type = ObjType_Frame;
                errval_t status = cap_retype(*retcap, node->cap.cap, base - node->cap.base,
                        type, size, 1);
                node->type = NodeType_Allocated;
                return status;
            }
        }
        node = node->next;
    }
    // Out of Memory!!
    return MM_ERR_FIND_NODE;
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
    return mm_alloc_aligned(mm, size, 1, retcap);
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
inline void mm_merge_mem_node_if_free(struct mm* mm, struct mmnode* node_first)
{
    if (node_first->type != NodeType_Free || node_first->next->type != NodeType_Free)
        return;
    struct mmnode* node_next = node_first->next;
    node_first->size += node_first->next->size;
    node_first->next = node_next->next;
    slab_free(&mm->slab, node_next);
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
    struct mmnode* node = mm->head;
    // Find node
    while (node != NULL && !capcmp(node->cap.cap, cap))
        node = node->next;

    // This node does not exist!
    if (!node)
        return MM_ERR_FIND_NODE;

    // Merge with previous if the previous one is free
    // (We may need to merge with previous AND next node - see aligned alloc)
    node->type = NodeType_Free;
    if (node->next)
        mm_merge_mem_node_if_free(mm, node);
    if (node->prev)
        mm_merge_mem_node_if_free(mm, node->prev);
    //! $node may be invalid now!
    return cap_revoke(cap);
}
