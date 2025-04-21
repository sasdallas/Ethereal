/**
 * @file libkstructures/include/structs/tree.h
 * @brief Generic tree implementation
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef STRUCTS_TREE_H
#define STRUCTS_TREE_H

/**** INCLUDES ****/
#include <stdint.h>
#include <structs/list.h>

/**** TYPES ****/

// Special structure for a tree node
typedef struct _tree_node {
    void *value;                // The value of this node
    list_t *children;           // A list of children for this tree node.
    struct _tree_node *parent;  // The parent to this node
} tree_node_t;

// Tree itself
typedef struct _tree {
    char *name;             // Name of the tree (this is for your bookkeeping, occasionally tree functions will warn)
    size_t nodes;           // Nodes in total
    tree_node_t *root;      // Root node
} tree_t;

// Comparator for the tree
typedef int (*tree_comparator_t)(void *value, void *search); // Takes in the value of the node, and what was specified as the search - return non-zero on success.

/**** FUNCTIONS ****/

/**
 * @brief Create a new tree
 * @param name Optional name, this is for your records and can be accessed via @c tree->name
 * @returns Tree object
 */
tree_t *tree_create(char *name);

/**
 * @brief Set the parent node of a tree
 * @param tree The tree to set the node of
 * @param value The value of a parent node
 */
void tree_set_parent(tree_t *tree, void *value);

/**
 * @brief Insert child node
 * @param tree The tree to insert the node into
 * @param parent The parent node
 * @param node The node to insert 
 */
void tree_insert_child_node(tree_t *tree, tree_node_t *parent, tree_node_t *node);

/**
 * @brief Insert child to a parent
 * @param tree The tree to insert the child into
 * @param parent The parent node
 * @param value The value of the new node
 * @returns The node created
 */
tree_node_t *tree_insert_child(tree_t *tree, tree_node_t *parent, void *value);

/**
 * @brief Returns the amount of children a node has
 * @param node The node to count the children of
 */
size_t tree_count_children(tree_node_t *node);

/**
 * @brief Destructively remove a node. This does no merging and instead drops all children
 * @param tree The tree to remove the node from
 * @param node The node to remove
 */
void tree_delete(tree_t *tree, tree_node_t *node);

/**
 * @brief Remove a node, but move its children into the children of @c parent
 * @param tree The tree to remove the node from
 * @param parent The parent to move children into
 * @param node The node to remove
 */
void tree_remove_reparent(tree_t *tree, tree_node_t *parent, tree_node_t *node);

/**
 * @brief Remove a node, but move its children into parent's children
 * @param tree The tree to remove the node from
 * @param node The node to remove
 */
void tree_remove(tree_t *tree, tree_node_t *node);

/**
 * @brief Find something in a tree using a comparator (recursive part)
 * @param node The node to use for this search
 * @param search The value to look for
 * @param comparator The comparator to call
 * @returns NULL if not found, or the node if it was
 */
tree_node_t *tree_find_node(tree_node_t *node, void *search, tree_comparator_t comparator);

/**
 * @brief Find something in a tree using a comparator
 * @param tree The tree to search
 * @param value The value to look for
 * @param comparator The comparator to use. This takes in a value (of the node currently being searched) and the value that was specified in this function.
 * @returns The node found, or nothing
 */
tree_node_t *tree_find(tree_t *tree, void *value, tree_comparator_t comparator);

#endif