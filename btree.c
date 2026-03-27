#include <stdio.h>
#include <stdlib.h>

#include "btree.h"

// Helper: Create a new node
TreeNode* create_node(unsigned long key, void *data) {
    TreeNode *newNode = (TreeNode*)malloc(sizeof(TreeNode));
    if (!newNode) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }
    newNode->key = key;
    newNode->data = data;
    newNode->left = NULL;
    newNode->right = NULL;
    return newNode;
}

// Insertion
TreeNode* insert_node(TreeNode *root, unsigned long key, void *data) {
    // 1. If the tree is empty, return a new node
    if (root == NULL) {
        return create_node(key, data);
    }

    // 2. Otherwise, recur down the tree
    if (key < root->key) {
        root->left = insert_node(root->left, key, data);
    } else if (key > root->key) {
        root->right = insert_node(root->right, key, data);
    } else {
        // Key already exists. Update data, or handle collision as preferred.
        root->data = data; 
    }

    return root;
}

// Search
TreeNode* search_node(TreeNode *root, unsigned long key) {
    // Base Cases: root is null or key is present at root
    if (root == NULL || root->key == key) {
        return root;
    }

    // Key is greater than root's key
    if (root->key < key) {
        return search_node(root->right, key);
    }

    // Key is smaller than root's key
    return search_node(root->left, key);
}

// Helper for deletion: Find the node with minimum key value
TreeNode* find_min(TreeNode *node) {
    TreeNode *current = node;
    while (current && current->left != NULL) {
        current = current->left;
    }
    return current;
}

// Deletion
TreeNode* delete_node(TreeNode *root, unsigned long key) {
    if (root == NULL) return root;

    // Navigate to the node to be deleted
    if (key < root->key) {
        root->left = delete_node(root->left, key);
    } else if (key > root->key) {
        root->right = delete_node(root->right, key);
    } else {
        // Node found. 
        
        // Case 1: Node with only one child or no child
        if (root->left == NULL) {
            TreeNode *temp = root->right;
            free(root);
            return temp;
        } else if (root->right == NULL) {
            TreeNode *temp = root->left;
            free(root);
            return temp;
        }

        // Case 2: Node with two children
        // Get the inorder successor (smallest in the right subtree)
        TreeNode *temp = find_min(root->right);

        // Copy the inorder successor's content to this node
        root->key = temp->key;
        root->data = temp->data; // Copy data pointer

        // Delete the inorder successor
        root->right = delete_node(root->right, temp->key);
    }
    return root;
}

// Memory Cleanup
void free_tree(TreeNode *root) {
    if (root == NULL) return;
    
    // Post-order traversal for deletion
    free_tree(root->left);
    free_tree(root->right);
    free(root);
}

// Traversal (In-order)
void print_inorder(TreeNode *root) {
    if (root != NULL) {
        print_inorder(root->left);
        printf("Key: %lu\n", root->key);
        print_inorder(root->right);
    }
}
