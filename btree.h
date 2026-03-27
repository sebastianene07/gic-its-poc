#ifndef BTREE_H
#define BTREE_H

#include <stddef.h>

// Definition of the Tree Node
typedef struct TreeNode {
    unsigned long key;       // The specific key requested
    void *data;              // Pointer to generic data
    struct TreeNode *left;
    struct TreeNode *right;
} TreeNode;

// Function Prototypes

// Creates a new node
TreeNode* create_node(unsigned long key, void *data);

// Inserts a node into the tree. Returns the new root.
TreeNode* insert_node(TreeNode *root, unsigned long key, void *data);

// Searches for a node by key. Returns the node or NULL.
TreeNode* search_node(TreeNode *root, unsigned long key);

// Deletes a node by key. Returns the new root.
TreeNode* delete_node(TreeNode *root, unsigned long key);

// Frees the entire tree and its nodes.
// Note: Does not free the 'data' pointer content (user must handle that).
void free_tree(TreeNode *root);

// Helper to print the tree in-order (sorted by key)
void print_inorder(TreeNode *root);

#endif
