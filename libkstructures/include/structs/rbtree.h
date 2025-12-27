/**
 * @file libkstructures/include/structs/rbtree.h
 * @brief Red-black tree
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef STRUCTS_RBTREE_H
#define STRUCTS_RBTREE_H

/**** INCLUDES ****/
#include <stdint.h>

/**** DEFINITIONS ****/

#define RBTREE_BLACK 0
#define RBTREE_RED 1

/**** TYPES ****/

typedef struct rb_tree_node {
    void *key;
    void *value;
    unsigned char color;
    struct rb_tree_node *parent;
    struct rb_tree_node *left;
    struct rb_tree_node *right;
} rb_tree_node_t;

typedef struct rb_tree {
    struct rb_tree_node *root;
    int (*compare_fn)(rb_tree_node_t *n1, rb_tree_node_t *n2); // -1 when n1 < n2, 0 when n1 = n2, 1 when n1 > n2
} rb_tree_t;

/**** MACROS ****/

#define RB_TREE_INIT_NODE(n, k, v) ({ (n)->key = (k); (n)->value = (v); (n)->parent = (n)->left = (n)->right = NULL; (n)->color = RBTREE_BLACK; })

#define RB_TREE_INITIALIZER { .root = NULL, .compare_fn = NULL }
#define RB_TREE_INIT(t) ({ (t)->root = NULL; })

/**** FUNCTIONS ****/

/**
 * @brief Create a new red-black tree
 * @returns Red black tree
 */
rb_tree_t *rbtree_create();

/**
 * @brief Insert a new red-black tree node
 * @param tree The red-black tree to insert the node in
 * @param node The node to insert into the red-black tree
 */
void rbtree_insert(rb_tree_t *tree, rb_tree_node_t *node);

/**
 * @brief Search in a red-black tree
 * @param tree The red-black tree to saerch in
 * @param key The target key
 * @param search_fn Search function. If NULL default search will be used.
 */
rb_tree_node_t *rbtree_search(rb_tree_t *tree, void *key, int (*search_fn)(void *a, void *b));

/**
 * @brief Delete a node from a red-black tree
 * @param tree The red-black tree to delete from
 * @param node The node to delete
 */
void rbtree_delete(rb_tree_t *tree, rb_tree_node_t *node);



#endif