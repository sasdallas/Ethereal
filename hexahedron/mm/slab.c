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
#include <kernel/processor_data.h>
#include <string.h>

#define LOG(status, ...) dprintf_module(status, "MM:SLAB", __VA_ARGS__)

/* Cache for slabs */
static slab_cache_t *slab_cache = NULL;
static slab_cache_t *magazine_cache = NULL;
static slab_cache_t *magazine_list_cache = NULL; // Generated at slab_postSMPHook

/* UNCOMMENT TO ENABLE SLAB DEBUGGING */
// #define SLAB_DEBUG 1

/* Magazine macros */
#define MAGAZINE_POP(mag) mag->rounds[--mag->nrounds]
#define MAGAZINE_PUSH(mag, val) (mag)->rounds[(mag)->nrounds++] = (val)

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
    memset(&cache->depot_full, 0, sizeof(magazine_depot_t));
    memset(&cache->depot_empty, 0, sizeof(magazine_depot_t));

    if (magazine_list_cache) {
        cache->per_cpu_cache = slab_allocate(magazine_list_cache);
        assert(cache->per_cpu_cache);
        memset(cache->per_cpu_cache, 0, sizeof(cpu_magazine_cache_t) * processor_count);
    } else {
        cache->per_cpu_cache = NULL;
    }
}

/**
 * @brief Push to depot
 */
static void depot_push(magazine_depot_t *depot, magazine_t *mag) {
    spinlock_acquire(&depot->lock);
    mag->next = depot->head;
    depot->head = mag;
    spinlock_release(&depot->lock);
}

/**
 * @brief Pop from depot
 */
static magazine_t *depot_pop(magazine_depot_t *depot) {
    spinlock_acquire(&depot->lock);

    magazine_t *mag = NULL;
    if (depot->head) {
        mag = depot->head;
        depot->head = depot->head->next;
    }

    spinlock_release(&depot->lock);
    return mag;
}

/**
 * @brief Make a new slab for the cache
 */
static slab_t *slab_new(slab_cache_t *cache) {
    // TODO: improval required?
    slab_t *new_slab = vmm_map(NULL, cache->slab_size, VM_FLAG_ALLOC, MMU_FLAG_WRITE | MMU_FLAG_PRESENT);
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
 * @brief Allocate slow path
 */
void *slab_slowAllocate(slab_cache_t *cache, sa_flags_t flags) {
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
 * @brief Fast allocate from cache
 */
void *slab_allocateFast(slab_cache_t *cache, sa_flags_t flags) {
    if (!cache->per_cpu_cache) return NULL;

    cpu_magazine_cache_t *cpu_cache = &cache->per_cpu_cache[arch_current_cpu()];
    
    spinlock_acquire(&cpu_cache->lock);
    
    // Try loaded
    if (cpu_cache->loaded && cpu_cache->loaded->nrounds) {
        void *ret = MAGAZINE_POP(cpu_cache->loaded);
        spinlock_release(&cpu_cache->lock);
        return ret;
    }

    // Try previous and swap magazines
    if (cpu_cache->previous && cpu_cache->previous->nrounds) {
        magazine_t *swap = cpu_cache->loaded;
        cpu_cache->loaded = cpu_cache->previous;
        cpu_cache->previous = swap;

        void *ret = MAGAZINE_POP(cpu_cache->loaded);
        spinlock_release(&cpu_cache->lock);
        return ret;
    }

    // Depot steal?
    // NOTE: The ideas from this code are sourced from Astral (which in turn is modeled after the Solaris allocator)
    magazine_t *mag = depot_pop(&cache->depot_full);
    if (mag) {
        // Push the empty magazine into the empty depot
        if (cpu_cache->previous) depot_push(&cache->depot_empty, cpu_cache->previous);
        cpu_cache->previous = cpu_cache->loaded;
        cpu_cache->loaded = mag;

        void *o = MAGAZINE_POP(cpu_cache->loaded);
        spinlock_release(&cpu_cache->lock);
        return o;
    }


    spinlock_release(&cpu_cache->lock);
    return NULL;
    
}

/**
 * @brief Slab allocate with special flags
 * @param cache The cache to allocate from
 * @param flags Additional flags to allocate with
 */
void *slab_allocateFlags(slab_cache_t *cache, sa_flags_t flags) {
    void *ptr = slab_allocateFast(cache, flags);
    if (ptr) {
        if (cache->init) cache->init(cache, ptr);
        return ptr;
    }

    if (flags & SA_FAST) {
        LOG(ERR, "SA_FAST specified but fast path was exhausted.\n");
        return NULL;
    }

    return slab_slowAllocate(cache, flags); // possible sleeping via mutex

}

/**
 * @brief Allocate an object from a cache
 * @param cache The cache to allocate from
 */
void *slab_allocate(slab_cache_t *cache) {
    return slab_allocateFlags(cache, SA_DEFAULT);
}

/**
 * @brief Slow free
 */
void slab_freeSlow(slab_cache_t *cache, void *object) {
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
 * @brief Fast free
 */
int slab_freeFast(slab_cache_t *cache, void *object) {
    if (!cache->per_cpu_cache) return 0;

    cpu_magazine_cache_t *cpu_cache = &cache->per_cpu_cache[arch_current_cpu()];
    spinlock_acquire(&cpu_cache->lock);

    if (cpu_cache->loaded && cpu_cache->loaded->nrounds < MAGAZINE_SIZE) {
        MAGAZINE_PUSH(cpu_cache->loaded, object);
        spinlock_release(&cpu_cache->lock);
        return 1;
    }

    if (cpu_cache->previous && !cpu_cache->previous->nrounds) {
        // Empty previous cache, free to that
        // Swap caches
        magazine_t *tmp = cpu_cache->loaded;
        cpu_cache->loaded = cpu_cache->previous;
        cpu_cache->previous = tmp;

        MAGAZINE_PUSH(cpu_cache->loaded, object);
        spinlock_release(&cpu_cache->lock);
        return 1;
    }

    // Retrieve one from depot if possible
    magazine_t *new_empty = depot_pop(&cache->depot_empty);
    if (new_empty) {
        // Try to push previous magazine
        if (cpu_cache->previous) depot_push(&cache->depot_full, cpu_cache->previous);

        cpu_cache->previous = cpu_cache->loaded;
        cpu_cache->loaded = new_empty;

        MAGAZINE_PUSH(cpu_cache->loaded, object);
        spinlock_release(&cpu_cache->lock);
        return 1;
    }


    // Allocate a new magazine
    magazine_t *new = slab_allocate(magazine_cache);
    if (new) {
        if (cpu_cache->previous) depot_push(&cache->depot_full, cpu_cache->previous);

        cpu_cache->previous = cpu_cache->loaded;
        cpu_cache->loaded = new;
        MAGAZINE_PUSH(cpu_cache->loaded, object);
        spinlock_release(&cpu_cache->lock);
        return 1;
    }

    return 0;
}

/**
 * @brief Free an object to a cache
 * @param cache The cache which the object was allocated to
 * @param object The object which was allocated
 */
void slab_free(slab_cache_t *cache, void *object) {
    if (cache->deinit) cache->deinit(cache, object);
    if (slab_freeFast(cache, object)) return;
    return slab_freeSlow(cache, object);
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
    slab_cache = vmm_map(NULL, sizeof(slab_cache_t), VM_FLAG_ALLOC, MMU_FLAG_PRESENT | MMU_FLAG_WRITE);
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

/**
 * @brief Magainze initializer
 */
int magazine_initializer(slab_cache_t *cache, void *magazine) {
    magazine_t *m = (magazine_t*)magazine;
    m->next = NULL; m->nrounds = 0;
    return 0;
}

/**
 * @brief Reinitialize a cache, post-SMP init
 * @param cache The cache to reinitialize
 */
void slab_reinitializeCache(slab_cache_t *cache) {
#ifdef SLAB_DEBUG
    LOG(DEBUG, "Reinitializing cache '%s' for %d CPUs...\n", cache->name, processor_count);
#endif

    cache->per_cpu_cache = slab_allocate(magazine_list_cache);
    assert(cache->per_cpu_cache);
    memset(cache->per_cpu_cache, 0, sizeof(cpu_magazine_cache_t) * processor_count);
}

/**
 * @brief Post-SMP hook
 * 
 * Needed because slabs and the allocator must be initialized for collecting SMP data.
 */
void slab_postSMPInit() {
    // Initialize the magazine cache
    magazine_cache = slab_createCache("magazine cache", sizeof(magazine_t) + sizeof(void*)*MAGAZINE_SIZE, 0, magazine_initializer, NULL);
    magazine_list_cache = slab_createCache("magazine list cache", sizeof(cpu_magazine_cache_t) * processor_count, 0, NULL, NULL);
}