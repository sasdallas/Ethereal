/**
 * @file libkstructures/include/structs/node.h
 * @brief Node structure
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef STRUCTS_NODE_H
#define STRUCTS_NODE_H

/**** INCLUDES ****/
#include <stdint.h>

/**** TYPES ****/

// Node structure
typedef struct _node {
    struct _node *next;     // Next node
    struct _node *prev;     // Previous node
    void *value;            // Value of this node
    void *owner;            // Owner of this node
} node_t;

#endif