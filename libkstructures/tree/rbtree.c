/**
 * @file libkstructures/rb/rbtree.c
 * @brief Rbtree structure
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <structs/rbtree.h>
#include <stdlib.h>
#include <assert.h>

#define RBTREE_COMPARE(t, n1, n2) (((t)->compare_fn) ? (t)->compare_fn(n1, n2) : rbtree_compare_default(n1, n2))
#define RBTREE_COMPARE_VAL(n1, n2) ((search_fn) ? search_fn(n1, n2) : rbtree_compare_value_default(n1, n2)) 
#define RBTREE_COLOR(n) (((n) == NULL) ? RBTREE_BLACK : ((n)->color))

/**
 * @brief rbtree_compare_default
 */
static int rbtree_compare_default(rb_tree_node_t *n1, rb_tree_node_t *n2) {
    if (n1->key == n2->key) return 0;
    if (n1->key > n2->key) return 1;
    if (n2->key > n1->key) return -1;
    __builtin_unreachable();
}

/**
 * @brief rbtree_compare_value_default
 */
static int rbtree_compare_value_default(void *n1, void *n2) {
    if (n1 == n2) return 0;
    if (n1 > n2) return 1;
    if (n2 > n1) return -1;
    __builtin_unreachable();
}

/**
 * @brief Create a new red-black tree
 * @returns Red black tree
 */
rb_tree_t *rbtree_create() {
    rb_tree_t *resp = malloc(sizeof(rb_tree_t));
    RB_TREE_INIT(resp);
    return resp;
}

/**
 * @brief Left rotate
 */
static void rbtree_left_rotate(rb_tree_t *tree, rb_tree_node_t *x) {
    rb_tree_node_t *y = x->right;
    assert(y != NULL);

    // Turn y's left subtree into x's right subtree
    x->right = y->left;
    if (y->left)
        y->left->parent = x;

    // Link x's parent to y
    y->parent = x->parent;
    if (!x->parent) {
        tree->root = y;
    } else if (x == x->parent->left) {
        x->parent->left = y;
    } else {
        x->parent->right = y;
    }

    // Put x on y's left
    y->left = x;
    x->parent = y;
}

/**
 * @brief Right rotate
 */
static void rbtree_right_rotate(rb_tree_t *tree, rb_tree_node_t *x) {
    rb_tree_node_t *y = x->left;
    assert(y != NULL);

    // Turn y's right subtree into x's left subtree
    x->left = y->right;
    if (y->right)
        y->right->parent = x;

    // Link x's parent to y
    y->parent = x->parent;
    if (!x->parent) {
        tree->root = y;
    } else if (x == x->parent->left) {
        x->parent->left = y;
    } else {
        x->parent->right = y;
    }

    // Put x on y's right
    y->right = x;
    x->parent = y;
}


/**
 * @brief Fix insertion
 */
static void rbtree_do_fixing(rb_tree_t *tree, rb_tree_node_t *grand_parent, rb_tree_node_t *parent, rb_tree_node_t *node) {
    // If node is the left child of a right child (or vice versa) we need to do a rotation.
    if (node == parent->left && parent == grand_parent->right) {
        // Left child of a right child
        // Perform right rotation
        rbtree_right_rotate(tree, parent);

        // Match node and parent
        node = parent;
        parent = grand_parent->right;
    } else if (node == parent->right && parent == grand_parent->left) {
        // Right child of a left child
        // Perform left rotation
        rbtree_left_rotate(tree, parent);

        // Match node and parent
        node = parent;
        parent = grand_parent->left;
    }

    // Now we should swap their colors
    parent->color = RBTREE_BLACK;
    grand_parent->color = RBTREE_RED; // rbtree_insert will fix this if grand_parent is the tree root

    // Final rotations...
    if (grand_parent->left == parent) {
        rbtree_right_rotate(tree, grand_parent);
    } else {
        rbtree_left_rotate(tree, grand_parent);
    }
}

/**
 * @brief Insert a new red-black tree node
 * @param tree The red-black tree to insert the node in
 * @param node The node to insert into the red-black tree
 */
void rbtree_insert(rb_tree_t *tree, rb_tree_node_t *node) {
    if (!tree->root) {
        // Make it black and the root
        node->color = RBTREE_BLACK;
        node->left = node->right = node->parent = NULL;
        tree->root = node;
        return;
    }


    // Let's find the topmost.. (Astral for the *target idea because I saw it and it was so clean)
    rb_tree_node_t *parent = tree->root;
    rb_tree_node_t **target = NULL;
    while (1) {
        // We will do a comparison
        int r = RBTREE_COMPARE(tree, parent, node);
        assert(r != 0);
        if (r >= 1) target = &parent->left;
        if (r <= -1) target = &parent->right;
        if (*target == NULL) break;
        parent = *target;
    }

    // Okay, let's insert the node...
    *target = node;
    node->parent = parent;
    node->color = RBTREE_RED;

    // Now, the hard partt.
    // There are two cases for the node's uncle:
    // 1. The uncle is red. This means we need to recolor the parent and uncle to black and the grandparent to red, then keep iterating.
    // 2. Uncle is black. We need to do LR/RR (+ recolor) as appropriate
    for (;;) {
        if (RBTREE_COLOR(parent) == RBTREE_BLACK) break; // Finished! No violations detected.

        // Get the current uncle of the node
        rb_tree_node_t *grand_parent = parent->parent;
        assert(grand_parent);
        
        rb_tree_node_t *uncle = grand_parent->left;
        if (uncle == parent) uncle = grand_parent->right;

        // Let's see
        if (RBTREE_COLOR(uncle) == RBTREE_BLACK) {
            rbtree_do_fixing(tree, grand_parent, parent, node);
            break;
        }        
    
        parent->color = RBTREE_BLACK;
        uncle->color = RBTREE_BLACK;
        grand_parent->color = RBTREE_RED;
    }

    // Ensure root is always black
    tree->root->color = RBTREE_BLACK;
}


/**
 * @brief Search in a red-black tree
 * @param tree The red-black tree to saerch in
 * @param key The target key
 * @param search_fn Search function. If NULL default search will be used.
 */
rb_tree_node_t *rbtree_search(rb_tree_t *tree, void *key, int (*search_fn)(void *a, void *b)) {
    rb_tree_node_t *n = tree->root;
    while (n) {
        int res = RBTREE_COMPARE_VAL(n->key, key);
        if (res == 0) return n;

        if (res >= 1) {
            // n->key is greater than key
            n = n->left;
        } else {
            n = n->right;
        }
    }

    return NULL;
}

/**
 * @brief Get a successor to a node in an rbtree
 * @param n The node to get the successor of
 * 
 * The successor is a node that does not have a left child in the subtree  
 */
rb_tree_node_t *rbtree_successor(rb_tree_node_t *n) {
    if (n->right) {
        rb_tree_node_t *p = n->right;
        while (p->left) p = p->left;
        return p;
    } else {
        // Move up until we find it
        rb_tree_node_t *p = n->parent;
        while (p && n == p->right) { n = p; p = p->parent; }
        return p;
    }

}

/**
 * @brief rbtree_fix_double_black
 * 
 * This is stupid dumb recursion method
 */
static void rbtree_fix_double_black(rb_tree_t *tree, rb_tree_node_t *n) {
    if (n == tree->root) return;

    rb_tree_node_t *parent = n->parent;
    rb_tree_node_t *sibling = (parent->left == n ? parent->right : parent->left);
    if (!sibling) {
        // Double black pushed up
        rbtree_fix_double_black(tree, parent);
    } else {
        if (sibling->color == RBTREE_RED) {
            // The sibling is red
            parent->color = RBTREE_RED;
            sibling->color = RBTREE_BLACK;
            
            if (parent->left == sibling) {
                rbtree_right_rotate(tree, parent);
                sibling = parent->left;
            } else {
                rbtree_left_rotate(tree, parent);
                sibling = parent->right;
            }
        } else {
            // The sibling is black
            // Does the sibling have a red child?
            if (RBTREE_COLOR(sibling->left) == RBTREE_RED || RBTREE_COLOR(sibling->right) == RBTREE_RED) {
                // Yes it does
                if (RBTREE_COLOR(sibling->left) == RBTREE_RED) {
                    // The red sibling is on the left
                    if (parent->left == sibling) {
                        rbtree_right_rotate(tree, parent);
                        sibling->left->color = sibling->color;
                        sibling->color = parent->color;
                    } else {
                        rbtree_right_rotate(tree, sibling);
                        rbtree_left_rotate(tree, parent);
                        sibling->left->color = parent->color;
                    }
                } else {
                    // The red sibling is on the right
                    if (parent->left == sibling) {
                        rbtree_left_rotate(tree, sibling);
                        rbtree_right_rotate(tree, parent);
                        sibling->right->color = parent->color;
                    } else {
                        rbtree_left_rotate(tree, parent);
                        sibling->right->color = sibling->color;
                        sibling->color = parent->color;
                    }
                }

                parent->color = RBTREE_BLACK;
            } else {
                // No, there are 2 black children
                sibling->color = RBTREE_RED;
                if (parent->color == RBTREE_BLACK) {
                    rbtree_fix_double_black(tree, parent);
                } else {
                    parent->color = RBTREE_BLACK;
                }
            }
        }
    }
}

/**
 * @brief Delete a node from a red-black tree
 * @param tree The red-black tree to delete from
 * @param node The node to delete
 */
void rbtree_delete(rb_tree_t *tree, rb_tree_node_t *n) {
    // Deletion is very annoying...

    // Find the node that would replace this one (standard BST)
    rb_tree_node_t *u = NULL;
    if (n->left && n->right) u = rbtree_successor(n);
    else if (n->left == NULL && n->right == NULL) u = NULL;
    else if (n->left) u = n->left;
    else u = n->right;


    if (!u) {
        // n is a leaf
        if (tree->root == n) { tree->root = NULL; return; }

        if (n->color == RBTREE_RED) {
            // The node is red, therefore it shouldn't actually matter.
            if (n->parent->left == n) {
                n->parent->left = NULL;
            } else {
                n->parent->right = NULL;
            }
            return;
        }

        // The leaf is black
        // We need to fix the double blacks
        rbtree_fix_double_black(tree, n);
        if (n->parent->left == n) {
            n->parent->left = NULL;
        } else {
            n->parent->right = NULL;
        }

        return;
    }
    
    rb_tree_node_t *parent = n->parent;
    if (!n->left || !n->right) {
        // n has one child
        rb_tree_node_t *child = (n->left ? n->left : n->right);
        if (n == tree->root) {
            // n is the tree's root
            tree->root = child;
        } else if (parent->left == n) {
            parent->left = child;
        } else {
            parent->right = child;
        }

        child->parent = parent;
        child->color = RBTREE_BLACK;
        return;
    }

    // Two children
    // Move successor to where node was
    unsigned char c = u->color;
    rb_tree_node_t *u_parent = u->parent;
    rb_tree_node_t *u_left = u->left;
    rb_tree_node_t *u_right = u->right;
    u->parent = n->parent;
    u->color = n->color;
    u->left = n->left;
    u->right = n->right;
    n->left->parent = u;
    if (u_parent != n) n->right->parent = u;

    // Make the successor the parent effectively
    if (parent == NULL) tree->root = u;
    else if (parent->left == n) parent->left = u;
    else parent->right = u;

    // Move the node into the successor (or its parent)'s children
    if (u_parent == n) {
        u->right = n; n->parent = u;
    } else {
        u_parent->left = n; n->parent = u_parent;
    }

    // Finish updating all of n's values
    n->color = c;
    n->left = u_left;
    if (u_left) u_left->parent = n;
    n->right = u_right;
    if (u_right) u_right->parent = n;

    // Now, recurse.
    return rbtree_delete(tree, n);
}