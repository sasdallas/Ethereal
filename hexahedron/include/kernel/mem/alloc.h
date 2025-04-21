/**
 * @file hexahedron/include/kernel/mem/alloc.h
 * @brief Provides allocator definitions, supporting Hexahedron's multiple allocator system.
 * 
 * Hexahedron allows for a choice of allocators, similar to Linux.
 * We don't support multiple simultaneously, but at compile-time it is possibly to change the current in-use 
 * memory allocator.
 * 
 * Every memory allocator should contain:
 * - The main allocator logic
 * - Any header files needed
 * - All the support files it needs to interact with the architecture-based memory system.
 * - Certain elements to allow it to interop with Hexahedron, including panicking.
 * 
 * All of the above should be contained in a single directory (e.g. hexalloc or toaru_alloc).
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of Ethereal Operating System.
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

/* Allocator information structure */
typedef struct _allocator_info {
    char        name[128];          // lmao imagine allocating memory in an allocator
    uint32_t    version_major;      // Major version of allocator
    uint32_t    version_minor;      // Minor version of allocator
    int         support_valloc;     // Whether the allocator supports valloc().

    // TODO: More flags will be added
} allocator_info_t;

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
 * @brief Page-aligned allocator
 * @note This is optional. Set the allocator info part support_valloc to 0 to not provide.
 */
extern __attribute__((malloc)) void *alloc_valloc(size_t nbyte);

/**
 * @brief Internal allocator function for freeing memory.
 */
extern void alloc_free(void *ptr);

/**
 * @brief Get information on the allocator.
 * @note This will be called multiple times. Keep a local copy.
 */
allocator_info_t *alloc_getInfo();


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
 * @brief Page-aligned memory allocator
 * @param size The size to allocate.
 * 
 * @note Do not rely on this! Allocators can choose not to provide this!
 * @returns A pointer or crashes with an unimplemented exception.
 */
__attribute__((malloc)) void *kvalloc(size_t size);

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