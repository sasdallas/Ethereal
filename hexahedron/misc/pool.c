/**
 * @file hexahedron/misc/pool.c
 * @brief Pool system. Commonly used with memory/DMA allocations
 * 
 * @warning This system is very finnicky. It is best to use a static-allocated pool
 *          with a memory space that YOU control. If not possible, the kernel heap should work,
 *          but it very well may make the system entirely unstable. Who knows.
 * 
 * @warning You may not destroy a pool. Any pools created are final.
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <stdint.h>
#include <errno.h>
#include <string.h>

#include <kernel/misc/pool.h>
#include <kernel/panic.h>
#include <kernel/debug.h>
#include <kernel/mm/vmm.h>
#include <kernel/mm/alloc.h>

/**
 * @brief Create a new pool
 * @param name Optional name for debugging
 * @param chunk_size The size of each chunk in the pool
 * @param size The size of the pool. This size is FINAL. It must be divisible by chunk_size
 * @param addr The starting address of the pool. If NULL, depending on @c flags it will be allocated
 * @param flags The pool creation flags
 * @returns The new pool object or NULL if something is wrong.
 */
pool_t *pool_create(char *name, uintptr_t chunk_size, uintptr_t size, uintptr_t addr, int flags) {
    if (size % chunk_size != 0) {
        kernel_panic(KERNEL_BAD_ARGUMENT_ERROR, "pool");
        __builtin_unreachable();
    }

    pool_t *pool = kmalloc(sizeof(pool_t));
    pool->name = name;
    pool->chunk_size = chunk_size;
    
    pool->bitmap = kmalloc((size / chunk_size)); // !!!: Is this bad? 
    memset(pool->bitmap, 0, (size / chunk_size));

    pool->allocated = size;
    pool->used = 0;

    if (!(flags & POOL_NOLOCK)) pool->lock = spinlock_create("pool_lock");

    if (addr != (uintptr_t)NULL) {
        // Best hope this will work out okay
        pool->starting_addr = addr;
    } else {
        // Allocate the pool ourselves
        if (flags & POOL_DMA) assert(0 && "POOL_DMA not implemented");
        else pool->starting_addr = (uintptr_t)vmm_map(NULL, size, VM_FLAG_ALLOC, MMU_FLAG_RW | MMU_FLAG_PRESENT); // !!!: Oh boy
    }

    return pool;
}

// Set a frame/bit in the pool array
void pool_setFrame(pool_t *pool, int frame) {
    pool->bitmap[POOL_INDEX_BIT(frame)] |= ((unsigned)1 << POOL_OFFSET_BIT(frame)); 
}

// Clear a frame/bit in the pool array
void pool_clearFrame(pool_t *pool, int frame) {
    pool->bitmap[POOL_INDEX_BIT(frame)] &= ~((unsigned)1 << POOL_OFFSET_BIT(frame));
}

// Test whether a frame is set
int pool_testFrame(pool_t *pool, int frame) {
    return pool->bitmap[POOL_INDEX_BIT(frame)] & ((unsigned)1 << POOL_OFFSET_BIT(frame));
}

// Find the first free frame
uint32_t pool_findFirstFrame(pool_t *pool) {
    for (uint32_t i = 0; i < POOL_INDEX_BIT(pool->allocated / pool->chunk_size); i++) {
        if (pool->bitmap[i] != 0xFFFFFFFF) {
            // At least one bit is free. Which one?
            for (uint32_t bit = 0; bit < 32; bit++) {
                if (!(pool->bitmap[i] & ((unsigned)1 << bit))) {
                    return i * 4 * 8 + bit;
                }
            }
        }
    }

    // No free frame
    return -ENOMEM;
}

// Find first n frames
int pool_findFirstFrames(pool_t *pool, size_t n) {
    if (n == 0) return 0x0;
    if (n == 1) return pool_findFirstFrame(pool);

    for (uint32_t i = 0; i < POOL_INDEX_BIT(pool->allocated / pool->chunk_size); i++) {
        if (pool->bitmap[i] != 0xFFFFFFFF) {
            // At least one bit is free. Which one?
            for (uint32_t bit = 0; bit < 32; bit++) {
                if (!(pool->bitmap[i] & ((unsigned)1 << bit))) {
                    // Found it! But are there enough?
                    int starting_bit = i * 32 + bit;

                    uint32_t free_bits = 0;
                    for (uint32_t check = 0; check < n; check++) {
                        if (!pool_testFrame(pool, starting_bit + check)) free_bits++;
                        if (free_bits == n) return i * 4 * 8 + bit; // Yes, there are!
                    }
                }
            }
        }
    }

    // Not enough free frames
    return -ENOMEM;
}


/**
 * @brief Allocate a chunk from the pool
 * @param pool The pool to allocate from
 * @returns A pointer to the chunk or NULL if a chunk was not found.
 */
uintptr_t pool_allocateChunk(pool_t *pool) {
    spinlock_acquire(pool->lock);

    if (pool->allocated - pool->used < pool->chunk_size) {
        goto _oom;
    }

    int frame = pool_findFirstFrame(pool);
    if (frame == -ENOMEM) goto _oom;

    pool_setFrame(pool, frame);
    pool->used += pool->chunk_size;

    spinlock_release(pool->lock);

    return (uintptr_t)(frame * pool->chunk_size) + pool->starting_addr;

_oom:
    spinlock_release(pool->lock);
    dprintf(WARN, "Pool '%s' has run out of memory\n", pool->name);
    return (uintptr_t)NULL;
}


/**
 * @brief Free a chunk and return it to the pool
 * @param pool The pool to use
 * @param chunk The chunk that was allocated
 */
void pool_freeChunk(pool_t *pool, uintptr_t chunk) {
    spinlock_acquire(pool->lock);

    if (chunk < pool->starting_addr || chunk > pool->allocated + pool->starting_addr) {
        goto _bad_chunk;
    }


    uintptr_t chunk_frame = chunk - pool->starting_addr;
    if (chunk_frame % pool->chunk_size) {
        goto _bad_chunk;
    }

    int frame = (chunk_frame == 0x0) ? 0 : chunk_frame / pool->chunk_size;
    pool_clearFrame(pool, frame);
    pool->used -= pool->chunk_size;

    spinlock_release(pool->lock);
    return;

_bad_chunk:
    spinlock_release(pool->lock);
    dprintf(WARN, "pool_freeChunk received a bad chunk 0x%x\n", chunk);
    return;
}

/**
 * @brief Allocate @c chunks chunks from the pool
 * @param pool The pool to use
 * @param chunks The amount of chunks to allocate
 * @returns A pointer to the chunks or NULL if there was not enough space
 */
uintptr_t pool_allocateChunks(pool_t *pool, uintptr_t chunks) {
    if (chunks == 0) return (uintptr_t)NULL;
    if (chunks == 1) return pool_allocateChunk(pool);

    spinlock_acquire(pool->lock);

    if (pool->allocated - pool->used < pool->chunk_size * chunks) {
        dprintf(DEBUG, "Pool sanity error - detected a possible underflow in '%s'\n", pool->name);
        dprintf(DEBUG, "\tpool->allocated = 0x%x pool->used = 0x%x pool->chunk_size = 0x%x requested chunks = 0x%x\n", pool->allocated, pool->used, pool->chunk_size, chunks);
        goto _oom;
    }

    int frame = pool_findFirstFrames(pool, chunks);
    if (frame == -ENOMEM) goto _oom;

    for (uint32_t i = 0; i < chunks; i++) {
        pool_setFrame(pool, frame + i);
    }

    pool->used += pool->chunk_size * chunks;
    
    spinlock_release(pool->lock);
    return (uintptr_t)(frame * pool->chunk_size) + pool->starting_addr;

_oom:
    spinlock_release(pool->lock);
    dprintf(WARN, "Pool '%s' has run out of memory\n", pool->name);
    return (uintptr_t)NULL;
}

/**
 * @brief Free @c chunks chunks from the pool
 * @param pool The pool to use
 * @param chunk_start The starting pointer to the chunks as returned from @c pool_allocateChunks
 * @param chunks The amount of chunks to free
 */
void pool_freeChunks(pool_t *pool, uintptr_t chunk_start, uintptr_t chunks) {
    if (!chunks || !chunk_start) return;
    
    spinlock_acquire(pool->lock);
    if (chunk_start < pool->starting_addr || chunk_start > pool->allocated + pool->starting_addr) {
        goto _bad_chunk;
    }

    uintptr_t chunk_frame = chunk_start - pool->starting_addr;
    if (chunk_frame % pool->chunk_size) {
        goto _bad_chunk;
    }

    int frame = (chunk_frame == 0x0) ? 0 : chunk_frame / pool->chunk_size;

    for (uint32_t i = 0; i < chunks; i++) {
        pool_clearFrame(pool, frame + i);
    }

    pool->used -= chunks * pool->chunk_size;
    
    spinlock_release(pool->lock);
    return;

_bad_chunk:
    spinlock_release(pool->lock);
    dprintf(WARN, "pool_freeChunks received a bad chunk 0x%x\n", chunk_start);
    return;
}