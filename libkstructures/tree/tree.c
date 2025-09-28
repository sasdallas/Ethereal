/**
 * @file libkstructures/tree/tree.c
 * @brief Generic tree implementation
 * 
 * This implements a tree system that follows a basic N-ary tree
 * guideline.
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <structs/tree.h>
#include <stdlib.h>

/**
 * @brief Create a new tree
 * @param name Optional name, this is for your records and can be accessed via @c tree->name
 * @returns Tree object
 */
tree_t *tree_create(char *name) {
    // TODO: strdup the name? that is probably a very good idea
    tree_t *tree = malloc(sizeof(tree_t));
    tree->name = name;
    tree->nodes = 0;
    tree->root = NULL; // Setting root nodes is accomplished elsewhere
    return tree;
}

/**
 * @brief Create a new node (static)
 */
static tree_node_t *tree_create_node(void *value) {
    tree_node_t *node = malloc(sizeof(tree_node_t));
    node->value = value;
    node->children = list_create("tree node children");
    node->parent = NULL;
    return node;
}

/**
 * @brief Set the parent node of a tree
 * @param tree The tree to set the node of
 * @param value The value of a parent node
 */
void tree_set_parent(tree_t *tree, void *value) {
    tree_node_t *node = tree_create_node(value);
    tree->root = node;
    tree->nodes = 1; // Reset node count
}

/**
 * @brief Insert child node
 * @param tree The tree to insert the node into
 * @param parent The parent node
 * @param node The node to insert 
 */
void tree_insert_child_node(tree_t *tree, tree_node_t *parent, tree_node_t *node) {
    // Add to tree node count
    tree->nodes++;
    tree->nodes += tree_count_children(node); // Just in case this is called from ext.
    node->parent = parent;
    list_append(parent->children, node);
}

/**
 * @brief Insert child to a parent
 * @param tree The tree to insert the child into
 * @param parent The parent node
 * @param value The value of the new node
 * @returns The node created
 */
tree_node_t *tree_insert_child(tree_t *tree, tree_node_t *parent, void *value) {
    tree_node_t *node = tree_create_node(value);
    tree_insert_child_node(tree, parent, node);
    return node;
}

/**
 * @brief Returns the amount of children a node has
 * @param node The node to count the children of
 */
size_t tree_count_children(tree_node_t *node) {
    if (!node || !node->children) return 0; // Base case (recursive)

    size_t children = node->children->length; // Go based off of list length
    foreach(child, node->children) {
        children += tree_count_children((tree_node_t*)child->value);
    }
    return children;
}

/**
 * @brief Free a node and its children
 * @param node The node to free
 * @warning This doesn't update anything, just frees the children. 
 */
static void tree_node_free(tree_node_t *node) {
    if (!node) return;
    foreach(child, node->children) {
        tree_node_free((tree_node_t*)child->value);
    }
    free(node);
}

/**
 * @brief Destructively remove a node. This does no merging and instead drops all children
 * @param tree The tree to remove the node from
 * @param node The node to remove
 */
void tree_delete(tree_t *tree, tree_node_t *node) {
    // Check if the node has a parent, we have to remove this node from its children
    tree_node_t *parent = node->parent;
    if (!parent) {
        if (node == tree->root) {
            // oh this is the root node
            tree->root = NULL;
            tree->nodes = 0;
            tree_node_free(node);
            return;
        } else {
            // ok what
            return;
        }
    }

    // Drop the node and its children
    tree->nodes--;
    tree->nodes -= tree_count_children(node);

    // Delete it from the parent's children
    list_delete(parent->children, list_find(parent->children, node));

    // Free the node
    tree_node_free(node);
}

/**
 * @brief Remove a node, but move its children into the children of @c parent
 * @param tree The tree to remove the node from
 * @param parent The parent to move children into
 * @param node The node to remove
 */
void tree_remove_reparent(tree_t *tree, tree_node_t *parent, tree_node_t *node) {
    // We have to merge the old node's children into our children
    if (!tree) return;
    
    // Drop the old node from the parent's list
    node_t *n = list_find(parent->children, (void*)node);
    if (n) list_delete(parent->children, n);

    // If the node has a list of children..
    if (node->children->length) {
        foreach(child_node, node->children) {
            // Reparent
            child_node->owner = (void*)parent->children;
            ((tree_node_t*)child_node->value)->parent = parent;
            list_append_node(parent->children, child_node);
        }
    }
    
    // We can't call list_destroy as it would free the nodes
    free(node->children);
    free(node);
}

/**
 * @brief Remove a node, but move its children into parent's children
 * @param tree The tree to remove the node from
 * @param node The node to remove
 */
void tree_remove(tree_t *tree, tree_node_t *node) {
    tree_node_t *parent = (tree_node_t*)node->parent;
    if (!parent) {
        // It's not possible to toss children to nothing!
        // We're not abusive parents here.
        // TODO: Maybe we should start. Just call tree_delete?
        return;
    }

    // All we do is call tree_remove_reparent
    tree->nodes--;
    tree_remove_reparent(tree, parent, node);
}


/**
 * @brief Find something in a tree using a comparator (recursive part)
 * @param node The node to use for this search
 * @param search The value to look for
 * @param comparator The comparator to call
 * @returns NULL if not found, or the node if it was
 */
tree_node_t *tree_find_node(tree_node_t *node, void *search, tree_comparator_t comparator) {
    // First run the comparator
    if (comparator(node->value, search)) return node; // Found it!
    
    // Iterate through the list of children
    foreach(child, node->children) {
        tree_node_t *found = tree_find_node((tree_node_t*)child->value, search, comparator);
        if (found) return found;
    }

    // Nothing
    return NULL;
}

/**
 * @brief Find something in a tree using a comparator
 * @param tree The tree to search
 * @param value The value to look for
 * @param comparator The comparator to use. This takes in a value (of the node currently being searched) and the value that was specified in this function.
 * @returns The node found, or nothing
 */
tree_node_t *tree_find(tree_t *tree, void *value, tree_comparator_t comparator) {
    return tree_find_node(tree->root, value, comparator);
}