#ifndef _LIB_BPTREE_HEADER
#define _LIB_BPTREE_HEADER


#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

// Default order is 4.
#define BPTREE_ORDER 4

// Minimum order is necessarily 3.  We set the maximum
// order arbitrarily.  You may change the maximum order.
#define MIN_ORDER 3
#define MAX_ORDER 20


// TYPES.

/* Type representing the record
 * to which a given key refers.
 * In a real B+ tree system, the
 * record would hold data (in a database)
 * or a file (in an operating system)
 * or some other information.
 * Users can rewrite this part of the code
 * to change the type and content
 * of the value field.
 */
typedef void* bpt_record_t;
typedef size_t bpt_key_t;

/* Type representing a node in the B+ tree.
 * This type is general enough to serve for both
 * the leaf and the internal node.
 * The heart of the node is the array
 * of keys and the array of corresponding
 * pointers.  The relation between keys
 * and pointers differs between leaves and
 * internal nodes.  In a leaf, the index
 * of each key equals the index of its corresponding
 * pointer, with a maximum of order - 1 key-pointer
 * pairs.  The last pointer points to the
 * leaf to the right (or NULL in the case
 * of the rightmost leaf).
 * In an internal node, the first pointer
 * refers to lower nodes with keys less than
 * the smallest key in the keys array.  Then,
 * with indices i starting at 0, the pointer
 * at i + 1 points to the subtree with keys
 * greater than or equal to the key in this
 * node at index i.
 * The num_keys field is used to keep
 * track of the number of valid keys.
 * In an internal node, the number of valid
 * pointers is always num_keys + 1.
 * In a leaf, the number of valid pointers
 * to data is always num_keys.  The
 * last leaf pointer points to the next leaf.
 */

struct bpt_node {
    void* pointers[BPTREE_ORDER];
    bpt_key_t keys[BPTREE_ORDER-1];
    struct bpt_node* parent;
    bool is_leaf;
    int num_keys;
    struct bpt_node* next; // Used for queue.
};


typedef void (*fun_free_bpt_node_t)(void*, struct bpt_node*);
typedef void (*fun_free_record_t)(void*, bpt_record_t);
typedef struct bpt_node* (*fun_alloc_bpt_node_t)(void*);

struct bpt_mem
{
    fun_free_bpt_node_t free_node;
    fun_alloc_bpt_node_t alloc_node;
    fun_free_record_t free_record;
    void* data;
};

// FUNCTION PROTOTYPES.

// Output and utility.
struct bpt_node* bpt_find_leaf(struct bpt_node* root, bpt_key_t key );
bpt_record_t bpt_find(struct bpt_node* root, bpt_key_t key );

// Insertion.
struct bpt_node* bpt_make_node(struct bpt_mem* mem );
struct bpt_node* bpt_make_leaf(struct bpt_mem* mem );
void bpt_init_node(struct bpt_node* new_node);
int bpt_get_left_index(struct bpt_node* parent, struct bpt_node* left);
struct bpt_node* bpt_insert_into_leaf(struct bpt_node* leaf, bpt_key_t key, bpt_record_t pointer );
struct bpt_node* bpt_insert_into_leaf_after_splitting(struct bpt_mem* mem, struct bpt_node* root, struct bpt_node* leaf, bpt_key_t key,
                                        bpt_record_t pointer);
struct bpt_node* bpt_insert_into_node(struct bpt_node* root, struct bpt_node* parent,
        int left_index, bpt_key_t key, struct bpt_node* right);
struct bpt_node* bpt_insert_into_node_after_splitting(struct bpt_mem* mem, struct bpt_node* root, struct bpt_node* parent,
                                        int left_index,
        bpt_key_t key, struct bpt_node* right);
struct bpt_node* bpt_insert_into_parent(struct bpt_mem* mem, struct bpt_node* root, struct bpt_node* left, bpt_key_t key, struct bpt_node* right);
struct bpt_node* bpt_insert_into_new_root(struct bpt_mem* mem, struct bpt_node* left, bpt_key_t key, struct bpt_node* right);
struct bpt_node* bpt_start_new_tree(struct bpt_mem* mem, bpt_key_t key, bpt_record_t pointer);
struct bpt_node* bpt_insert(struct bpt_mem* mem, struct bpt_node* root, bpt_key_t key, bpt_record_t value );

// Deletion.

int bpt_get_neighbor_index(struct bpt_node* n );
struct bpt_node* bpt_adjust_root(struct bpt_mem* mem, struct bpt_node* root);
struct bpt_node* bpt_coalesce_nodes(struct bpt_mem* mem, struct bpt_node* root, struct bpt_node* n, struct bpt_node* neighbor,
                      int neighbor_index, bpt_key_t k_prime);
struct bpt_node* bpt_redistribute_nodes(struct bpt_node* root, struct bpt_node* n, struct bpt_node* neighbor,
                          int neighbor_index,
        bpt_key_t k_prime_index, bpt_key_t k_prime);
struct bpt_node* bpt_delete_entry(struct bpt_mem* mem, struct bpt_node* root, struct bpt_node* n, bpt_key_t key, void * pointer );
struct bpt_node* bpt_delete(struct bpt_mem* mem, struct bpt_node* root, bpt_key_t key );
struct bpt_node* bpt_remove_entry_from_node(struct bpt_node* n, bpt_key_t key, struct bpt_node* pointer);
void bpt_destroy_tree_nodes(struct bpt_mem* mem, struct bpt_node* root);
struct bpt_node* bpt_destroy_tree(struct bpt_mem* mem, struct bpt_node* root);

#endif
