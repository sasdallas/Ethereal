/**
 * @file hexahedron/include/kernel/mm/slab.h
 * @brief Slab
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef KERNEL_MM_SLAB_H
#define KERNEL_MM_SLAB_H

/**** INCLUDES ****/
#include <stdint.h> 
#include <kernel/misc/mutex.h>

/**** DEFINITIONS ****/
#define SLAB_MAX_FREE 2

/**** TYPES ****/

typedef struct magazine {
    struct magazine *next;          // Next magazine in the list
    size_t nrounds;                 // Number of rounds
    void *rounds[];                 // Rounds
} magazine_t;

typedef struct cpu_magazine_cache {
    magazine_t *loaded;             // Loaded magazine
    magazine_t *previous;           // Previous magazine
    mutex_t mut;                    // Mutex
} cpu_magazine_cache_t;

typedef struct slab {
    struct slab *next;              // Next slab in the list
    struct slab *prev;
    void **free_list;               // Free list
    size_t free_cnt;                // Free count of objects
} slab_t;

struct slab_cache;

/**
 * @brief Slab constructor
 * 
 * Called when a new object is returned by @c slab_allocate
 * @param cache The cache which created the object
 * @param object The object to initialize
 */
typedef int (*slab_initializer_t)(struct slab_cache *cache, void *object);

/**
 * @brief Slab deinitializer
 * 
 * Called when an object is being freed by @c slab_free
 * @param cache The cache which created the object
 * @param object The object to deinitialize
 */
typedef int (*slab_deinitialize_t)(struct slab_cache *cache, void *object);

typedef struct slab_cache {
    slab_t *slabs_full;             // Full slabs
    slab_t *slabs_partial;          // Partially filled slabs
    slab_t *slabs_free;             // Totally free slabs

    // Object details
    size_t slab_object_size;        // Size of each slab object
    size_t slab_object_cnt;         // Amount of objects in a slab
    size_t slab_object_alignment;   // Alignment
    size_t slab_object_real_size;   // Real slab object size
    size_t slab_size;               // Size of a slab

    // Constructor and destructor
    slab_initializer_t init;
    slab_deinitialize_t deinit;

    // Misc.
    char *name;                     // Name of the caches
    mutex_t mut;                    // Mutex
    size_t mem_usage;               // Memory usage
} slab_cache_t;

/**** FUNCTIONS ****/

/**
 * @brief Initialize slab allocator
 */
void slab_init();

/**
 * @brief Create a new slab cache
 * 
 * Slab caches are great for repeated object allocations.
 * 
 * @param name The name of the cache
 * @param size The size of the objects to allocate
 * @param alignment Alignment of the objects (leave as 0 for no alignment)
 * @param initializer Optional initializer
 * @param deinitializer Optional deinitializer
 */
slab_cache_t *slab_createCache(char *name, size_t size, size_t alignment, slab_initializer_t initializer, slab_deinitialize_t deinitializer);

/**
 * @brief Allocate an object from a cache
 * @param cache The cache to allocate from
 */
void *slab_allocate(slab_cache_t *cache);

/**
 * @brief Free an object to a cache
 * @param cache The cache which the object was allocated to
 * @param object The object which was allocated
 */
void slab_free(slab_cache_t *cache, void *object);

/**
 * @brief Destroy a slab cache (freeing all of its objects)
 * @param cache The cache to destroy
 */
void slab_destroyCache(slab_cache_t *cache);

/**
 * @brief Print cache statistics
 */
void slab_stats(slab_cache_t *cache);

#endif