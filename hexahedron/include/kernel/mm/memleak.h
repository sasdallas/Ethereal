/**
 * @file hexahedron/include/kernel/mm/memleak.h
 * @brief Kernel memory leak detector
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef KERNEL_MM_MEMLEAK_H
#define KERNEL_MM_MEMLEAK_H

/**** INCLUDES ****/
#include <stdint.h>
#include <kernel/misc/spinlock.h>
#include <structs/rbtree.h>
#include <structs/list.h>

/**** DEFINITIONS ****/

#define MEMLEAK_WHITE 0     // Objects that could be memory leaks
#define MEMLEAK_GREY 1      // Objects that are known not to be memory leaks
#define MEMLEAK_BLACK 2     // Objects that have no references to other objects in the white set

/**** TYPES ****/

typedef struct memleak_object {
    spinlock_t lck;         // Lock
    rb_tree_node_t node;   // Tree node
    node_t lnode;           // Linked list node
    void *frames[10];       // Stack frames
    void *ptr;              // Pointer
    size_t size;            // Size
    uint8_t paint;          // Paint
} memleak_object_t;

/**** FUNCTIONS ****/

/**
 * @brief Memory leak checker init
 */
void memleak_init();

/**
 * @brief Add object from allocation
 * @param ptr The allocation pointer
 * @param size The allocation size
 */
void memleak_alloc(void *ptr, size_t size);

/**
 * @brief Remove object from allocation
 * @param ptr Pointer to the object
 */
void memleak_free(void *ptr);

#endif