/**
 * @file hexahedron/include/kernel/mm/alloc.h
 * @brief Allocator
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef KERNEL_MM_ALLOC_H
#define KERNEL_MM_ALLOC_H

/**** INCLUDES ****/
#include <string.h>

/**** MACROS ****/
#define kzalloc(a) ({ void *p = kmalloc(a); memset(p, 0, a); p; })


/**** FUNCTIONS ****/

__attribute__((malloc)) void *kmalloc(size_t size);
__attribute__((malloc)) void *krealloc(void *ptr, size_t size);
__attribute__((malloc)) void *kcalloc(size_t nobj, size_t size);
void kfree(void *ptr);

/**
 * @brief Get allocator bytes in use (cache)
 */
size_t alloc_used();

/**
 * @brief Print allocator statistics
 */
void alloc_stats();

/**
 * @brief Initialize allocator
 */
void alloc_init();

#endif