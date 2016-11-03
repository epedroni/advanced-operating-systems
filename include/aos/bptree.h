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
typedef void* bpt_record;

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
typedef struct bpt_node {
    void* pointers[BPTREE_ORDER];
    int keys[BPTREE_ORDER-1];
    struct bpt_node * parent;
    bool is_leaf;
    int num_keys;
    struct bpt_node * next; // Used for queue.
} bpt_node;


// FUNCTION PROTOTYPES.

// Output and utility.
bpt_node * bpt_find_leaf( bpt_node * root, int key, bool verbose );
bpt_record * bpt_find( bpt_node * root, int key, bool verbose );

// Insertion.
bpt_node * bpt_make_node( void );
bpt_node * bpt_make_leaf( void );
int bpt_get_left_index(bpt_node * parent, bpt_node * left);
bpt_node * bpt_insert_into_leaf( bpt_node * leaf, int key, bpt_record * pointer );
bpt_node * bpt_insert_into_leaf_after_splitting(bpt_node * root, bpt_node * leaf, int key,
                                        bpt_record * pointer);
bpt_node * bpt_insert_into_node(bpt_node * root, bpt_node * parent,
        int left_index, int key, bpt_node * right);
bpt_node * bpt_insert_into_node_after_splitting(bpt_node * root, bpt_node * parent,
                                        int left_index,
        int key, bpt_node * right);
bpt_node * bpt_insert_into_parent(bpt_node * root, bpt_node * left, int key, bpt_node * right);
bpt_node * bpt_insert_into_new_root(bpt_node * left, int key, bpt_node * right);
bpt_node * bpt_start_new_tree(int key, bpt_record * pointer);
bpt_node * bpt_insert( bpt_node * root, int key, bpt_record* value );

// Deletion.

int bpt_get_neighbor_index( bpt_node * n );
bpt_node * bpt_adjust_root(bpt_node * root);
bpt_node * bpt_coalesce_nodes(bpt_node * root, bpt_node * n, bpt_node * neighbor,
                      int neighbor_index, int k_prime);
bpt_node * bpt_redistribute_nodes(bpt_node * root, bpt_node * n, bpt_node * neighbor,
                          int neighbor_index,
        int k_prime_index, int k_prime);
bpt_node * bpt_delete_entry( bpt_node * root, bpt_node * n, int key, void * pointer );
bpt_node * bpt_delete( bpt_node * root, int key );
bpt_node * bpt_remove_entry_from_node(bpt_node * n, int key, bpt_node * pointer);
void bpt_destroy_tree_nodes(bpt_node * root);
bpt_node * bpt_destroy_tree(bpt_node * root);

#endif
