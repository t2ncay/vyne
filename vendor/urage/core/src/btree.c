#include "btree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static LeafNode *get_leaf_node(Pager *pager, page_num_t page_num) {
    return (LeafNode *)pager_get_page(pager, page_num);
}

static InternalNode *get_internal_node(Pager *pager, page_num_t page_num) {
    return (InternalNode *)pager_get_page(pager, page_num);
}

static void initialize_leaf_node(void *page) {
    LeafNode *node = (LeafNode *)page;
    node->header.type = NODE_LEAF;
    node->header.is_root = 0;
    node->header.parent = INVALID_PAGE;
    node->num_cells = 0;
}

static void initialize_internal_node(void *page) {
    InternalNode *node = (InternalNode *)page;
    node->header.type = NODE_INTERNAL;
    node->header.is_root = 0;
    node->header.parent = INVALID_PAGE;
    node->num_keys = 0;
}

BTree *btree_create(Pager *pager) {
    BTree *tree = malloc(sizeof(BTree));
    if (!tree) return NULL;
    
    tree->pager = pager;
    
    // Check if database already has a root
    if (pager->num_pages > 0) {
        // Existing database - root is page 0
        tree->root_page_num = 0;
    } else {
        // New database - create root
        tree->root_page_num = pager_allocate_page(pager);
        void *root_node = pager_get_page(pager, tree->root_page_num);
        initialize_leaf_node(root_node);
        ((LeafNode *)root_node)->header.is_root = 1;
    }
    
    return tree;
}

static DB_Result leaf_node_insert(LeafNode *node, uint32_t key, page_num_t value) {
    if (node->num_cells >= LEAF_NODE_MAX_CELLS) {
        return DB_FULL;
    }
    
    // Find insertion point
    int insertion_point = 0;
    while (insertion_point < node->num_cells && node->keys[insertion_point] < key) {
        insertion_point++;
    }
    
    // Shift cells
    for (int i = node->num_cells; i > insertion_point; i--) {
        node->keys[i] = node->keys[i - 1];
        node->values[i] = node->values[i - 1];
    }
    
    // Insert
    node->keys[insertion_point] = key;
    node->values[insertion_point] = value;
    node->num_cells++;
    
    return DB_SUCCESS;
}

static DB_Result leaf_node_find(LeafNode *node, uint32_t key, page_num_t *value) {
    // Binary search
    int left = 0;
    int right = node->num_cells - 1;
    
    while (left <= right) {
        int mid = (left + right) / 2;
        if (node->keys[mid] == key) {
            *value = node->values[mid];
            return DB_SUCCESS;
        } else if (node->keys[mid] < key) {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }
    
    return DB_NOT_FOUND;
}

static DB_Result split_leaf_node(BTree *tree, LeafNode *old_node, page_num_t old_page_num) {
    // Create new node
    page_num_t new_page_num = pager_allocate_page(tree->pager);
    if (new_page_num == INVALID_PAGE) return DB_FULL;
    
    LeafNode *new_node = get_leaf_node(tree->pager, new_page_num);
    initialize_leaf_node((void *)new_node);
    
    // Split at midpoint
    int split_point = LEAF_NODE_MAX_CELLS / 2;
    
    // Copy second half to new node
    for (int i = split_point; i < LEAF_NODE_MAX_CELLS; i++) {
        new_node->keys[i - split_point] = old_node->keys[i];
        new_node->values[i - split_point] = old_node->values[i];
    }
    new_node->num_cells = LEAF_NODE_MAX_CELLS - split_point;
    old_node->num_cells = split_point;
    
    // Update parent
    uint32_t new_key = new_node->keys[0];
    
    if (old_node->header.is_root) {
        // Create new root
        page_num_t root_page_num = pager_allocate_page(tree->pager);
        if (root_page_num == INVALID_PAGE) return DB_FULL;
        
        InternalNode *root = get_internal_node(tree->pager, root_page_num);
        initialize_internal_node(root);
        root->header.is_root = 1;
        
        root->children[0] = old_page_num;
        root->keys[0] = new_key;
        root->children[1] = new_page_num;
        root->num_keys = 1;
        
        old_node->header.is_root = 0;
        old_node->header.parent = root_page_num;
        new_node->header.parent = root_page_num;
        
        tree->root_page_num = root_page_num;
    } else {
        // Insert into parent
        InternalNode *parent = get_internal_node(tree->pager, old_node->header.parent);
        
        // Find insertion point in parent
        int insert_index = 0;
        while (insert_index <= parent->num_keys && 
               parent->children[insert_index] != old_page_num) {
            insert_index++;
        }
        
        // Shift keys and children
        for (int i = parent->num_keys; i > insert_index; i--) {
            parent->keys[i] = parent->keys[i - 1];
            parent->children[i + 1] = parent->children[i];
        }
        
        // Insert new key and child
        parent->keys[insert_index] = new_key;
        parent->children[insert_index + 1] = new_page_num;
        parent->num_keys++;
        
        new_node->header.parent = old_node->header.parent;
    }
    
    return DB_SUCCESS;
}

DB_Result btree_insert(BTree *tree, uint32_t key, page_num_t value) {
    page_num_t current_page = tree->root_page_num;
    void *node = pager_get_page(tree->pager, current_page);
    NodeHeader *header = (NodeHeader *)node;
    
    // Navigate to leaf
    while (header->type == NODE_INTERNAL) {
        InternalNode *internal = (InternalNode *)node;
        
        // Find correct child
        int child_index = 0;
        while (child_index < internal->num_keys && key >= internal->keys[child_index]) {
            child_index++;
        }
        
        current_page = internal->children[child_index];
        node = pager_get_page(tree->pager, current_page);
        header = (NodeHeader *)node;
    }
    
    // Insert into leaf
    LeafNode *leaf = (LeafNode *)node;
    
    // Check if leaf is full
    if (leaf->num_cells >= LEAF_NODE_MAX_CELLS) {
        DB_Result result = split_leaf_node(tree, leaf, current_page);
        if (result != DB_SUCCESS) return result;
        
        // Retry insertion (now with new structure)
        return btree_insert(tree, key, value);
    }
    
    return leaf_node_insert(leaf, key, value);
}

DB_Result btree_find(BTree *tree, uint32_t key, page_num_t *value) {
    printf("Debug: btree_find(key=%u)\n", key);
    
    page_num_t current_page = tree->root_page_num;
    void *node = pager_get_page(tree->pager, current_page);
    NodeHeader *header = (NodeHeader *)node;
    
    printf("Debug: Root page=%u, node type=%d\n", current_page, header->type);
    
    // Navigate to leaf
    while (header->type == NODE_INTERNAL) {
        InternalNode *internal = (InternalNode *)node;
        
        // Find correct child
        int child_index = 0;
        while (child_index < internal->num_keys && key >= internal->keys[child_index]) {
            child_index++;
        }
        
        current_page = internal->children[child_index];
        printf("Debug: Going to child page %u at index %d\n", current_page, child_index);
        
        node = pager_get_page(tree->pager, current_page);
        header = (NodeHeader *)node;
    }
    
    // Search in leaf
    LeafNode *leaf = (LeafNode *)node;
    printf("Debug: Leaf node has %u cells\n", leaf->num_cells);
    
    for (int i = 0; i < leaf->num_cells; i++) {
        printf("Debug: Cell %d: key=%u, value=%u\n", i, leaf->keys[i], leaf->values[i]);
        if (leaf->keys[i] == key) {
            *value = leaf->values[i];
            printf("Debug: Found key %u at cell %d, value=%u\n", key, i, *value);
            return DB_SUCCESS;
        }
    }
    
    printf("Debug: Key %u not found in leaf\n", key);
    return DB_NOT_FOUND;
}

void btree_print_node(Pager *pager, page_num_t page_num, int level) {
    void *node = pager_get_page(pager, page_num);
    NodeHeader *header = (NodeHeader *)node;
    
    for (int i = 0; i < level; i++) printf("  ");
    
    if (header->type == NODE_LEAF) {
        LeafNode *leaf = (LeafNode *)node;
        printf("Leaf[%d]: ", page_num);
        for (int i = 0; i < leaf->num_cells; i++) {
            printf("%d ", leaf->keys[i]);
        }
        printf("\n");
    } else {
        InternalNode *internal = (InternalNode *)node;
        printf("Internal[%d]: ", page_num);
        for (int i = 0; i < internal->num_keys; i++) {
            printf("%d ", internal->keys[i]);
        }
        printf("\n");
        
        for (int i = 0; i <= internal->num_keys; i++) {
            btree_print_node(pager, internal->children[i], level + 1);
        }
    }
}

void btree_print(BTree *tree) {
    printf("B-Tree Structure:\n");
    btree_print_node(tree->pager, tree->root_page_num, 0);
}

// Add this function to btree.c
DB_Result btree_delete(BTree *tree, uint32_t key) {
    if (!tree || !tree->pager) return DB_ERROR;
    
    printf("Debug: btree_delete(key=%u)\n", key);
    
    page_num_t current_page = tree->root_page_num;
    void *node = pager_get_page(tree->pager, current_page);
    NodeHeader *header = (NodeHeader *)node;
    
    // Navigate to leaf
    while (header->type == NODE_INTERNAL) {
        InternalNode *internal = (InternalNode *)node;
        
        int child_index = 0;
        while (child_index < internal->num_keys && key >= internal->keys[child_index]) {
            child_index++;
        }
        
        current_page = internal->children[child_index];
        node = pager_get_page(tree->pager, current_page);
        header = (NodeHeader *)node;
    }
    
    // Now at leaf node
    LeafNode *leaf = (LeafNode *)node;
    
    // Find the key
    int found_index = -1;
    for (int i = 0; i < leaf->num_cells; i++) {
        if (leaf->keys[i] == key) {
            found_index = i;
            break;
        }
    }
    
    if (found_index == -1) {
        printf("Debug: Key %u not found in leaf\n", key);
        return DB_NOT_FOUND;
    }
    
    // Remove the key by shifting all cells after it left
    printf("Debug: Removing key %u at index %d\n", key, found_index);
    
    for (int i = found_index; i < leaf->num_cells - 1; i++) {
        leaf->keys[i] = leaf->keys[i + 1];
        leaf->values[i] = leaf->values[i + 1];
    }
    
    leaf->num_cells--;
    printf("Debug: Leaf now has %u cells\n", leaf->num_cells);
    
    // Force flush to disk
    pager_flush_page(tree->pager, current_page);
    
    return DB_SUCCESS;
}