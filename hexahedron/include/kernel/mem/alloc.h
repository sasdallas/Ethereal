/**
 * @file hexahedron/include/kernel/mem/alloc.h
 * @brief Provides allocator definitions, supporting Hexahedron's multiple allocator system.
 * 
 * @warning
 *  Hexahedron used to allow for multiple allocator systems.
 *  This system will be phased out soon.
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_MEM_ALLOC_H
#define KERNEL_MEM_ALLOC_H


/**** INCLUDES ****/
#include <stdint.h>
#include <sys/time.h>
#include <time.h>

/**** TYPES ****/


/**** MACROS ****/

// NOTE: These will not be used everywhere in the kernel.

/* Allocate and zero out allocation */ 
#define kzalloc(a) ({ void *p = alloc_malloc(a); memset(p, 0, a); p; })

/**** FUNCTIONS ****/


/* THE ALLOCATOR SHOULD PROVIDE THESE FUNCTIONS */
/* @see alloc.c FOR THE CALLERS OF THESE FUNCTIONS */

/**
 * @brief Internal allocator function for getting memory.
 */
extern __attribute__((malloc)) void *alloc_malloc(size_t nbyte);

/**
 * @brief Internal allocator function for reallocating memory.
 */
extern __attribute__((malloc)) void *alloc_realloc(void *ptr, size_t nbyte);

/**
 * @brief Internal allocator function for getting memory of a specific size for a specific amount of times.
 */
extern __attribute__((malloc)) void *alloc_calloc(size_t elements, size_t size);

/**
 * @brief Internal allocator function for freeing memory.
 */
extern void alloc_free(void *ptr);


/* ALLOCATOR MANAGEMENT SYSTEM (alloc.c) WILL PROVIDE THESE FUNCTIONS */

/**
 * @brief Allocate kernel memory
 * @param size The size of the allocation
 * 
 * @returns A pointer. It will crash otherwise.
 */
__attribute__((malloc)) void *kmalloc(size_t size);

/**
 * @brief Reallocate kernel memory
 * @param ptr A pointer to the previous structure
 * @param size The new size of the structure.
 * 
 * @returns A pointer. It will crash otherwise.
 */
__attribute__((malloc)) void *krealloc(void *ptr, size_t size);

/**
 * @brief Contiguous allocation function
 * @param elements The amount of elements to allocate
 * @param size The size of each element
 * 
 * @returns A pointer. It will crash otherwise.
 */
__attribute__((malloc)) void *kcalloc(size_t elements, size_t size);

/**
 * @brief Free kernel memory
 * @param ptr A pointer to the previous memory
 */
void kfree(void *ptr);

/**
 * @brief valloc?
 */
int alloc_canHasValloc();

#endif