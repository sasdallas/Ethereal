/**
 * @file hexahedron/mm/slab.c
 * @brief Kernel memory slab allocator
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/mm/vmm.h>
#include <kernel/mm/slab.h>
#include <kernel/debug.h>
#include <assert.h>
#include <kernel/panic.h>
#include <kernel/misc/util.h>
#include <string.h>

#define LOG(status, ...) dprintf_module(status, "MM:SLAB", __VA_ARGS__)

/* Cache for slabs */
static slab_cache_t *slab_cache = NULL;



/* UNCOMMENT TO ENABLE SLAB DEBUGGING */
#define SLAB_DEBUG 1


/**
 * @brief (internal) Initialize slab cache
 */
static void slab_initCache(slab_cache_t *cache, char *name, size_t size, size_t alignment, slab_initializer_t initializer, slab_deinitialize_t deinitializer) {
    if (!alignment) alignment = 1;
    cache->name = name;
    MUTEX_INIT(&cache->mut);
    cache->slab_object_size = size;
    cache->slab_object_alignment = alignment;

    // Ensure alignment is valid (must be a power of two)
    if (alignment > 1) assert(!(alignment & 1));
    cache->slab_object_real_size = ALIGN_UP(size, alignment);

    // TODO: There's got to be a better way to do this and conserve memory
    cache->slab_size = PAGE_ALIGN_UP(cache->slab_object_real_size +  sizeof(slab_t));
    cache->slab_object_cnt = (cache->slab_size - sizeof(slab_t)) / cache->slab_object_real_size;

    
    cache->init = initializer;
    cache->deinit = deinitializer;
    cache->slabs_partial = cache->slabs_full = cache->slabs_free = NULL;
}

/**
 * @brief Make a new slab for the cache
 */
static slab_t *slab_new(slab_cache_t *cache) {
    // TODO: improval required?
    slab_t *new_slab = vmm_map(NULL, cache->slab_size, VM_FLAG_ALLOC, MMU_FLAG_RW | MMU_FLAG_KERNEL | MMU_FLAG_PRESENT);
    if (!new_slab) return NULL;

    memset(new_slab, 0, sizeof(slab_t));

    new_slab->free_cnt = cache->slab_object_cnt;
    new_slab->next = new_slab->prev  = NULL;
    

    // Link up the freelist
    for (uintptr_t i = 0; i < cache->slab_object_real_size * cache->slab_object_cnt; i += cache->slab_object_real_size) {
        void **nxt = (void**)(((uintptr_t)new_slab + sizeof(slab_t)) + i);
        *nxt = new_slab->free_list;
        new_slab->free_list = nxt;
    }

#ifdef SLAB_DEBUG
    LOG(DEBUG, "New slab created for cache \"%s\" (%d size), using %d bytes of memory\n", cache->name, cache->slab_object_size, cache->slab_size);
#endif

    
    cache->mem_usage += cache->slab_size;

    return new_slab;
}

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
slab_cache_t *slab_createCache(char *name, size_t size, size_t alignment, slab_initializer_t initializer, slab_deinitialize_t deinitializer) {
    // Get a new cache from the slab cahce
    slab_cache_t *c = slab_allocate(slab_cache);
    assert(c);

    slab_initCache(c, name, size, alignment, initializer, deinitializer);
   
#ifdef SLAB_DEBUG
    LOG(DEBUG, "New slab cache '%s' created (object_size = %d object_alignment = %d)\n", name, size, alignment);
#endif

    return c;
}

/**
 * @brief Allocate an object from a cache
 * @param cache The cache to allocate from
 */
void *slab_allocate(slab_cache_t *cache) {
    mutex_acquire(&cache->mut);

    // Can we take from the partial list?
    slab_t *slab = cache->slabs_partial;
    if (!slab) slab = cache->slabs_free;
    
    if (!slab) {
        // Create a new slab and add it to the partially filled list
        slab = slab_new(cache);

        if (!slab) {
            mutex_release(&cache->mut);
            return NULL;
        }

        cache->slabs_free = slab;
    }

    assert(slab->free_cnt);

    // Pop an object from the freelist
    void *o = slab->free_list;
    slab->free_list = *(void**)o;
    slab->free_cnt--;

    // Move between the lists for the slab
    if (slab->free_cnt == 0) {
        // Out of space, remove from the previous list and track
        if (slab->next) slab->next->prev = slab->prev;
        if (slab->prev) slab->prev->next = slab->next;

        if (slab == cache->slabs_partial) cache->slabs_partial = slab->next;
        if (slab == cache->slabs_free) cache->slabs_free = slab->next;

        slab->prev = NULL;
        slab->next = cache->slabs_full;
        if (cache->slabs_full) cache->slabs_full->prev = slab;
        cache->slabs_full = slab;
    } else if (slab->free_cnt == cache->slab_object_cnt - 1) {
        // Partially full now
        if (slab->next) slab->next->prev = slab->prev;
        if (slab->prev) slab->prev->next = slab->next;
        if (slab == cache->slabs_free) cache->slabs_free = slab->next;
        slab->prev = NULL;
        slab->next = cache->slabs_partial;
        if (cache->slabs_partial) cache->slabs_partial->prev = slab;
        cache->slabs_partial = slab;
    }

    mutex_release(&cache->mut);

    if (cache->init) cache->init(cache, o);
    return o;
}

/**
 * @brief Free an object to a cache
 * @param cache The cache which the object was allocated to
 * @param object The object which was allocated
 */
void slab_free(slab_cache_t *cache, void *object) {
    if (cache->deinit) cache->deinit(cache, object);
    mutex_acquire(&cache->mut);

    // Get the slab
    slab_t *slab = (slab_t*)ALIGN_DOWN((uintptr_t)object, PAGE_SIZE-1);
    
    // TODO: Poison support would be nice..
    // Return object to freelist
    *(void**)object = slab->free_list;
    slab->free_list = (void**)object;
    slab->free_cnt++;

    if (slab->free_cnt == cache->slab_object_cnt) {
        // Return to free list
        if (slab->next) slab->next->prev = slab->prev;
        if (slab->prev) slab->prev->next = slab->next;

        if (slab == cache->slabs_partial) cache->slabs_partial = slab->next;
        if (slab == cache->slabs_full) cache->slabs_full = slab->next;

        slab->next = cache->slabs_free;
        slab->prev = NULL;
        if (cache->slabs_free) cache->slabs_free->prev = slab;
        cache->slabs_free = slab;

        // We may want to return some slabs to the VMM
        if (cache != slab_cache) {
            size_t free_count = 0;
            slab_t *s = cache->slabs_free;
            while (s) { free_count++; s = s->next; }

            while (free_count > SLAB_MAX_FREE) {
                slab_t *last = cache->slabs_free;
                if (!last) break;
                while (last->next) last = last->next;

                if (last->prev) last->prev->next = NULL;
                else cache->slabs_free = NULL;

                last->prev = last->next = NULL;

#ifdef SLAB_DEBUG
                LOG(DEBUG, "Freeing excess slab for cache \"%s\"\n", cache->name);
#endif

                // Unmap the slab pages
                vmm_unmap((void*)last, cache->slab_size);

                cache->mem_usage -= cache->slab_size;

                free_count--;
            }
        }
    } else if (slab->free_cnt == 1) {
        // Return to partial list
        if (slab->next) slab->next->prev = slab->prev;
        if (slab->prev) slab->prev->next = slab->next;
        if (slab == cache->slabs_full) cache->slabs_full = slab->next;
        slab->prev = NULL;
        slab->next = cache->slabs_partial;

        if (cache->slabs_partial) cache->slabs_partial->prev = slab;

        cache->slabs_partial = slab;
    }

    mutex_release(&cache->mut);
}

/**
 * @brief Destroy a slab cache (freeing all of its objects)
 * @param cache The cache to destroy
 */
void slab_destroyCache(slab_cache_t *cache) {
    STUB();
}

/**
 * @brief Initialize slab allocator
 */
void slab_init() {
    slab_cache = vmm_map(NULL, sizeof(slab_cache_t), VM_FLAG_ALLOC, MMU_FLAG_PRESENT | MMU_FLAG_RW | MMU_FLAG_KERNEL);
    assert(slab_cache);
    slab_initCache(slab_cache, "slab cache", sizeof(slab_cache_t), 1, NULL, NULL);

    LOG(INFO, "Slab allocator initialized\n");
}

/**
 * @brief Print cache statistics
 */
void slab_stats(slab_cache_t *cache) {
    LOG(DEBUG, "DUMP OF CACHE \"%s\" statistics:\n", cache->name);
    LOG(DEBUG, "Slab size: %d bytes (object size/align: %d/%d)\n", cache->slab_size, cache->slab_object_size, cache->slab_object_alignment);

    // Count
    size_t free_cnt = 0;
    size_t used_cnt = 0;
    size_t full_cnt = 0;

    for (slab_t *s = cache->slabs_free; s; free_cnt++, s = s->next);
    for (slab_t *s = cache->slabs_partial; s; used_cnt++, s = s->next);
    for (slab_t *s = cache->slabs_full; s; full_cnt++, s = s->next);

    LOG(DEBUG, "Free slabs: %d (using %d bytes of memory)\n", free_cnt, free_cnt * cache->slab_size);
    LOG(DEBUG, "Partially full slabs: %d (using %d bytes of memory)\n", used_cnt, used_cnt * cache->slab_size);
    LOG(DEBUG, "Full slabs: %d (using %d bytes of memory)\n", full_cnt, full_cnt * cache->slab_size);

}