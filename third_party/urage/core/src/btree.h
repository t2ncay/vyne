#ifndef BTREE_H
#define BTREE_H

#include "constants.h"
#include "pager.h"

// B-Tree node types
typedef enum {
    NODE_INTERNAL,
    NODE_LEAF
} NodeType;

// B-Tree node header (common for all nodes)
typedef struct {
    NodeType type;
    uint32_t is_root;
    page_num_t parent;
} NodeHeader;

// Leaf node structure
#define LEAF_NODE_MAX_CELLS 31  // (PAGE_SIZE - header) / (key + value)

typedef struct {
    NodeHeader header;
    uint32_t num_cells;
    uint32_t keys[LEAF_NODE_MAX_CELLS];
    page_num_t values[LEAF_NODE_MAX_CELLS];  // Pointers to data pages
} LeafNode;

// Internal node structure
#define INTERNAL_NODE_MAX_KEYS 30  // (PAGE_SIZE - header) / (key + child)
#define INTERNAL_NODE_MAX_CHILDREN 31

typedef struct {
    NodeHeader header;
    uint32_t num_keys;
    uint32_t keys[INTERNAL_NODE_MAX_KEYS];
    page_num_t children[INTERNAL_NODE_MAX_CHILDREN];
} InternalNode;

// B-Tree handle
typedef struct {
    Pager *pager;
    page_num_t root_page_num;
} BTree;

// B-Tree operations
BTree *btree_create(Pager *pager);
DB_Result btree_insert(BTree *tree, uint32_t key, page_num_t value);
DB_Result btree_find(BTree *tree, uint32_t key, page_num_t *value);
DB_Result btree_delete(BTree *tree, uint32_t key);
void btree_print(BTree *tree);

#endif