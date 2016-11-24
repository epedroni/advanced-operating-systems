#ifndef LIBBARRELFISH_PAGING_DATASTRUCT_H
#define LIBBARRELFISH_PAGING_DATASTRUCT_H

#define PAGING_STORE_AS_LIST
//#define PAGING_STORE_AS_BPTREE

// Unused here...
struct paging_region {
    lvaddr_t base_addr;
    lvaddr_t current_addr;
    size_t region_size;
    // TODO: if needed add struct members for tracking state
};

enum virtual_block_type {
    VirtualBlock_Free,
    VirtualBlock_Allocated,
    VirtualBlock_Paged
};

/*************************************
 * Data structure for storing mem blocks
 *************************************/

struct vm_block {
    enum virtual_block_type type;
    size_t size;
    int map_flags;  // Only needed when lazy-allocated
    struct capref mapping;
#ifdef PAGING_STORE_AS_LIST
    struct vm_block* next;
    struct vm_block* prev;
    lvaddr_t start_address;
#endif
};

// LIST SPECIFIC CODE
#ifdef PAGING_STORE_AS_LIST

typedef struct vm_block* vm_block_key_t;
typedef struct vm_block* vm_block_struct_t;

#define ADDRESS_FROM_VM_BLOCK_KEY(key) (key->start_address)
#define CHECK_DATA_CORRECTNESS(st, verbose)
#define PAGING_SLAB_REFILL(st)
#endif

// BP-TREE SPECIFIC
#ifdef PAGING_STORE_AS_BPTREE
#include <aos/bptree.h>

struct vm_block_key
{
    struct bpt_node* node;
    int index;
};

typedef struct vm_block_key vm_block_key_t;
typedef struct bpt_node  bpt_node_t;

typedef struct vm_block_struct
{
    bpt_node_t* root;
    struct bpt_mem mem;
    // Slab for allocating tree nodes
    struct bpt_node slab_nodes_init_buffer[15];
    struct slab_allocator slab_nodes;
} vm_block_struct_t;


#define ADDRESS_FROM_VM_BLOCK_KEY(key) (key.node->keys[key.index])
void check_tree_correctness(struct bpt_node* root, bool verbose);
#define CHECK_DATA_CORRECTNESS(st, verbose) check_tree_correctness(st->blocks.root, verbose)
#define PAGING_SLAB_REFILL(st) {if (!slab_has_freecount(&st->blocks.slab_nodes, 3*6+2)) \
    aos_slab_refill(&st->blocks.slab_nodes);}
#endif

struct paging_state;
struct vm_block* find_free_block_with_size(struct paging_state *st, size_t min_size, vm_block_key_t* key);

struct vm_block* find_block_before(struct paging_state *st,
    lvaddr_t before_address, vm_block_key_t* key);
struct vm_block* add_block_after(struct paging_state *st, vm_block_key_t original,
    lvaddr_t ad_address, vm_block_key_t* new_key);
struct vm_block* create_root(struct paging_state* st, size_t start_address);
void vm_block_merge_free_neighbors(struct paging_state* st, vm_block_key_t virtual_addr);


#endif
