#include <aos/aos.h>
#include <aos/except.h>
#include <aos/paging.h>

#ifdef PAGING_STORE_AS_BPTREE

#include <aos/slab.h>
#include "threads_priv.h"
#include <mm/mm.h>

#include <stdio.h>
#include <string.h>

void print_node(struct bpt_node* n);
void print_node(struct bpt_node* n)
{
    if (n->parent)
    {
        int parent_idx = 0;
        for (; parent_idx < n->parent->num_keys && n->parent->pointers[parent_idx] != n; ++parent_idx);
        assert(parent_idx < n->parent->num_keys+1);
        if (parent_idx == 0)
            debug_printf("-infinity <= values\n");
        else
            debug_printf("0x%08x <= values\n", n->parent->keys[parent_idx-1]);
        debug_printf("values < 0x%08x\n", n->parent->keys[parent_idx]);
    }
    else
        debug_printf("[no parent, root node]\n");

    for (int i = 0; i < n->num_keys; ++i)
        debug_printf("Key %d: value 0x%08x\n", (int)i, n->keys[i]);
}

void check_tree_correctness(struct bpt_node* root, bool verbose)
{
    if (verbose)
        debug_printf("###### check_tree_correctness\n");
    bpt_node_t* n = root;
    while (!n->is_leaf)
		n = n->pointers[0];
    size_t prev_addr = 0;
    struct vm_block* prev_block = NULL;
    int i;
    while (n != NULL) {
        i = 0;
		for (; i < n->num_keys; ++i) {
            struct vm_block* va = (struct vm_block*)n->pointers[i];
            if (verbose)
            {
                const char* type = "???";
                switch (va->type)
                {
                    case VirtualBlock_Free:
                        type = "FREE";
                        break;
                    case VirtualBlock_Allocated:
                        type = "ALLOCATED";
                        break;
                    case VirtualBlock_Paged:
                        type = "PAGED";
                        break;
                }
                debug_printf("[0x%08x[%d]][Addr0x%08x][Block0x%08x] Size is 0x%08x [%s]\n",
                    (int)n, (int)i, (int)n->keys[i], (int)va, (int)va->size,
                    type);
            }
            if (prev_block)
                assert(prev_addr + prev_block->size == n->keys[i]);
            prev_block = va;
            prev_addr = n->keys[i];
		}
        if (verbose)
            debug_printf(">> Move to next Node (0x%08x -> 0x%08x)\n",
                (int)n, (int)n->pointers[BPTREE_ORDER - 1]);
		n = n->pointers[BPTREE_ORDER - 1];
	}
}

/**
 * \brief Finds a free block with given size
 */
struct vm_block* find_free_block_with_size(struct paging_state *st, size_t min_size, vm_block_key_t* key)
{
    // Loop through all nodes
    bpt_node_t* n = st->blocks.root;
    while (!n->is_leaf)
		n = n->pointers[0];
    int i;
    while (n != NULL) {
        i = 0;
		for (; i < n->num_keys; ++i) {
            struct vm_block* va = (struct vm_block*)n->pointers[i];
            if (va->type != VirtualBlock_Free ||
                va->size < min_size)
                continue;
            key->index = i;
            key->node = n;
            return va;
		}
		n = n->pointers[BPTREE_ORDER - 1];
	}
    return NULL;
}

/**
 * \brief Finds the last block before given $address
 */
struct vm_block* find_block_before(struct paging_state *st,
    lvaddr_t address, vm_block_key_t* key)
{
    struct bpt_node* n = bpt_find_leaf(st->blocks.root, address);
    if (!n)
        return NULL;

    assert(n->is_leaf);
    int i;
    for (i = 0; i < n->num_keys-1 && n->keys[i+1] <= address; ++i);

    if (n->keys[i] > address) // Not found on this leaf. Then means that it does not exist.
        return NULL;

    assert(i+1 >= n->num_keys || address < n->keys[i+1]);

    key->index = i;
    key->node = n;
    return (struct vm_block*)n->pointers[i];
}

struct vm_block* add_block_after(struct paging_state* st,
    vm_block_key_t original, lvaddr_t at_address, vm_block_key_t* new_key)
{
    assert(!bpt_find(st->blocks.root, at_address) &&
         "Can't add block here. There is already one!");

    struct vm_block* original_block = original.node->pointers[original.index];
    struct vm_block* new_block = slab_alloc(&st->slabs);
    size_t original_size = original_block->size;
    assert(original_size + ADDRESS_FROM_VM_BLOCK_KEY(original) > at_address);
    //original_block->size = at_address - ADDRESS_FROM_VM_BLOCK_KEY(original);
    //new_block->size = original_size + ADDRESS_FROM_VM_BLOCK_KEY(original) - at_address;

    st->blocks.root = bpt_insert(&st->blocks.mem, st->blocks.root, at_address, new_block);

    struct bpt_node* n = bpt_find_leaf(st->blocks.root, at_address);
    new_key->node = n;
    for (new_key->index = 0; new_key->index < n->num_keys && at_address != n->keys[new_key->index]; ++new_key->index);
    assert(new_key->index < n->num_keys && "Inserted node not found?!");

    assert(bpt_find(st->blocks.root, at_address) &&
         "Insertion failed?!");

    return new_block;
}

static struct bpt_node* callback_bpt_alloc_node(void* data)
{
    struct paging_state* st = (struct paging_state*)data;
    return slab_alloc(&st->blocks.slab_nodes);
}

static void callback_bpt_free_node(void* data, bpt_node_t* node)
{
    struct paging_state* st = (struct paging_state*)data;
    return slab_free(&st->blocks.slab_nodes, node);
}

static void callback_bpt_free_record(void* data, bpt_record_t record)
{
    struct paging_state* st = (struct paging_state*)data;
    return slab_free(&st->slabs, record);
}

struct vm_block* create_root(struct paging_state* st, size_t start_address)
{
    slab_init(&st->blocks.slab_nodes,
        sizeof(bpt_node_t), NULL);
    SLAB_SET_NAME(&st->blocks.slab_nodes, "PagingBPTree");
    slab_grow(&st->blocks.slab_nodes,
        st->blocks.slab_nodes_init_buffer, sizeof(st->blocks.slab_nodes_init_buffer));

    st->blocks.mem.alloc_node = callback_bpt_alloc_node;
    st->blocks.mem.free_node = callback_bpt_free_node;
    st->blocks.mem.free_record = callback_bpt_free_record;
    st->blocks.mem.data = st;

    struct vm_block* block = slab_alloc(&st->slabs);
    assert(block);
    st->blocks.root = bpt_insert(&st->blocks.mem, NULL, start_address, block);
    return block;
}

static struct bpt_node* bpt_find_prev_leaf(struct bpt_node* n)
{
    // Go up until we are no longer the leftmost leaf
    // "To the left, to the left..."
    struct bpt_node* parent = n->parent;
    while (parent && parent->pointers[0] == n)
    {
        n = parent;
        parent = n->parent;
    }
    // We climbed up to the root and found nothing on the left.
    if (!parent)
        return NULL;

    // Now find the child before n
    int i;
    for (i = 0; i < parent->num_keys+1; ++i)
        if (parent->pointers[i] == n)
            break;
    assert(i > 0);
    assert(i != parent->num_keys+1);
    n = parent->pointers[i-1];
    assert(n);

    // Go down always to the right
    while (!n->is_leaf)
        n = n->pointers[n->num_keys];
    return n;
}

/**
 * Merges with neighbors if they are free too.
 * Invalidates any reference to block or block key.
 */
void vm_block_merge_free_neighbors(struct paging_state* st, vm_block_key_t key)
{
    // Find current block
    assert(key.node);
    assert(key.index < key.node->num_keys);
    struct vm_block* block = key.node->pointers[key.index];
    int block_key = key.node->keys[key.index];

    // Find next block
    int block_next_key = -1;
    struct vm_block* block_next = NULL;
    struct bpt_node* n = key.node;
    int i = key.index;
    if (i == n->num_keys)
    {
        n = n->pointers[BPTREE_ORDER - 1];
        i = 0;
    }
    else
        ++i;
    if (n && i < n->num_keys)
    {
        block_next = n->pointers[i];
        block_next_key = n->keys[i];
    }

    // Find prev block
    struct vm_block* block_prev = NULL;
    if (key.index)
    {
        block_prev = key.node->pointers[key.index-1];
    }
    else
    {
        n = bpt_find_prev_leaf(key.node);
        if (n)
        {
            assert(n->num_keys);
            block_prev = n->pointers[n->num_keys-1];
        }
    }

    // Do the merge
    if (block_next && block_next->type == VirtualBlock_Free)
    {
        block->size += block_next->size;
        st->blocks.root = bpt_delete(&st->blocks.mem, st->blocks.root, block_next_key);
        assert(st->blocks.root);
    }
    if (block_prev && block_prev->type == VirtualBlock_Free)
    {
        block_prev->size += block->size;
        st->blocks.root = bpt_delete(&st->blocks.mem, st->blocks.root, block_key);
        assert(st->blocks.root);
    }
}

#endif
