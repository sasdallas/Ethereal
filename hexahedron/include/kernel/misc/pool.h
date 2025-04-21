/**
 * @file hexahedron/include/kernel/misc/pool.h
 * @brief Header for the memory pool system
 * 
 * @warning This system is very finnicky. It is best to use a static-allocated pool
 *          with a memory space that YOU control. If not possible, the kernel heap should work,
 *          but it very well may make the system entirely unstable. Who knows.
 * 
 * @warning You may not destroy a pool. Any pools created are final.
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_MISC_POOL_H
#define KERNEL_MISC_POOL_H

/**** INCLUDES ****/

#include <stdint.h>
#include <stddef.h>
#include <kernel/misc/spinlock.h>

/**** DEFINITIONS ****/

// Pool creation flags (TODO: expand on this)
#define POOL_DEFAULT        0       // Allocate via mem_sbrk
#define POOL_DMA            1       // Allocate via mem_allocateDMA
#define POOL_NOLOCK         2       // Do not use a pool lock

/**** TYPES ****/

typedef struct _pool {
    spinlock_t  *lock;          // Lock for the pool
    char        *name;          // Optional name for debugging

    uint32_t    *bitmap;        // Bitmap. The pool system uses a bitmap similar to the way the PMM works
    uintptr_t   chunk_size;     // Size of each chunk in the pool
    uintptr_t   starting_addr;  // Starting address of the pool

    uintptr_t   allocated;      // Amount of bytes alloacted to the pool
    uintptr_t   used;           // Amount of bytes used in the pool
} pool_t;

/**** MACROS ****/
#define POOL_INDEX_BIT(a) (a / (8 * 4))
#define POOL_OFFSET_BIT(a) (a % (8 * 4))

/**** FUNCTIONS ****/

/**
 * @brief Create a new pool
 * @param name Optional name for debugging
 * @param chunk_size The size of each chunk in the pool
 * @param size The size of the pool. This size is FINAL. It must be divisible by chunk_size
 * @param addr The starting address of the pool. If NULL, depending on @c flags it will be allocated
 * @param flags The pool creation flags
 * @returns The new pool object or NULL if something is wrong.
 */
pool_t *pool_create(char *name, uintptr_t chunk_size, uintptr_t size, uintptr_t addr, int flags);

/**
 * @brief Allocate a chunk from the pool
 * @param pool The pool to allocate from
 * @returns A pointer to the chunk or NULL if a chunk was not found.
 */
uintptr_t pool_allocateChunk(pool_t *pool);

/**
 * @brief Free a chunk and return it to the pool
 * @param pool The pool to use
 * @param chunk The chunk that was allocated
 */
void pool_freeChunk(pool_t *pool, uintptr_t chunk);

/**
 * @brief Allocate @c chunks chunks from the pool
 * @param pool The pool to use
 * @param chunks The amount of chunks to allocate
 * @returns A pointer to the chunks or NULL if there was not enough space
 */
uintptr_t pool_allocateChunks(pool_t *pool, uintptr_t chunks);

/**
 * @brief Free @c chunks chunks from the pool
 * @param pool The pool to use
 * @param chunk_start The starting pointer to the chunks as returned from @c pool_allocateChunks
 * @param chunks The amount of chunks to free
 */
void pool_freeChunks(pool_t *pool, uintptr_t chunk_start, uintptr_t chunks);

#endif