/**
 * @file hexahedron/mm/alloc.c
 * @brief Memory allocator
 * 
 * Styled off of Linux, using the slab allocator as a base
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
#include <kernel/debug.h>
#include <kernel/misc/util.h>
#include <assert.h>
#include <kernel/panic.h>
#include <string.h>
#include <kernel/processor_data.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "MM:ALLOC", __VA_ARGS__)

/* Edit this to change the amount of caches the allocator uses */
#define ALLOC_CACHES    14
static size_t __alloc_cache_sizes[ALLOC_CACHES] = {
    16, 32, 64, 128, 256, 512, 1024, 2048, 4096,
    8192, 16384, 32768, 65536, 131072
};

/* Allocator caches */
slab_cache_t *alloc_caches[ALLOC_CACHES] = { 0 };

/* UNCOMMENT TO ENABLE TRACKING */
#define ENABLE_TRACKING 1

/* Magic values */
#define ALLOC_MAGIC_ALLOCATED       0xCAFEBABE
#define ALLOC_MAGIC_FREE            0xC0FFEE11

/* Allocation header */
/* !!!: Dirty hack to ensure accurate padding */

#define ALLOC_HEADER   uint32_t magic; \
                        uintptr_t function; \
                        size_t cache_size; \
                        size_t alloc_size; 

struct __alloc_header {ALLOC_HEADER};

typedef struct alloc_header {
    uint32_t magic;
    uintptr_t function; // only used if ENABLE_TRACKING is set
    size_t cache_size;
    size_t alloc_size; 
    uint8_t padding[sizeof(struct __alloc_header) % 16];
} alloc_header_t;

/* Get header function */
#define ALLOC_GETHEADER(x) (alloc_header_t*)((uintptr_t)(x) - sizeof(alloc_header_t))

/* In use */
size_t alloc_in_use = 0;

/**
 * @brief Object initializer
 */
static int __kmalloc_initializer(slab_cache_t *cache, void *object) {
    alloc_header_t *hdr = (alloc_header_t*)object;
    hdr->magic = ALLOC_MAGIC_ALLOCATED;
    hdr->cache_size = cache->slab_object_size;

#ifdef ENABLE_POISON
    #error "TODO"
#endif

    return 0;
}

/**
 * @brief Object deinitializer
 */
static int __kmalloc_deinitializer(slab_cache_t *cache, void *object) {
    alloc_header_t *hdr = (alloc_header_t*)object;
    assert(hdr->magic == ALLOC_MAGIC_ALLOCATED);
    hdr->magic = ALLOC_MAGIC_FREE;

#ifdef ENABLE_POISON
    #error "TODO"
#endif
    return 0;
}

/**
 * @brief Get a cache which an allocation belongs to
 * 
 * Returns NULL if no cache to handle the allocation is present
 */
static slab_cache_t *alloc_getCache(size_t size) {
    for (int i = 0; i < ALLOC_CACHES; i++) {
        if (size <= __alloc_cache_sizes[i]) {
            return alloc_caches[i];
        }
    }

    return NULL;
}

/**
 * @brief kmalloc flags
 */
__attribute__((malloc)) void *kmalloc_flags(size_t size, kma_flags_t kmaflags) {
    size = size + sizeof(alloc_header_t);

    if (current_cpu->current_thread) {
        if ((current_cpu->current_thread->status & THREAD_STATUS_SLEEPING) != 0) {
            LOG(ERR, "Thread %p, function %p - attempted to allocate memory while sleeping.\n", current_cpu->current_thread, __builtin_return_address(0));
            assert(0);
        } 
    }

    // Get a cache object
    slab_cache_t *cache = alloc_getCache(size);
    if (cache) {
        // Try to allocate
        void *m = slab_allocate(cache);
        if (!m) return NULL;

        alloc_header_t *h = (alloc_header_t*)m;
        #ifdef ENABLE_TRACKING
            h->function = (uintptr_t)__builtin_return_address(0);
        #endif
        h->alloc_size = size - sizeof(alloc_header_t);

        alloc_in_use += h->alloc_size;

        return (void*)((uintptr_t)m + sizeof(alloc_header_t));
    } else {
        // Problem: this allocation is bigger than we can actually handle.
        // Solution: VMM, this one's for you
        LOG(DEBUG, "(%p) Big allocation: %d bytes\n", __builtin_return_address(0), size);
        void *m = vmm_map(NULL, size, VM_FLAG_ALLOC, MMU_FLAG_RW | MMU_FLAG_PRESENT | MMU_FLAG_KERNEL);
        if (!m) return NULL;

        alloc_header_t *h = (alloc_header_t*)m;
        h->cache_size = size;
        h->magic = ALLOC_MAGIC_ALLOCATED;
        h->alloc_size = size - sizeof(alloc_header_t);

    #ifdef ENABLE_TRACKING
        h->function = (uintptr_t)__builtin_return_address(0);
    #endif

        alloc_in_use += h->alloc_size;
        return (void*)((uintptr_t)m + sizeof(alloc_header_t));
    }

}

/**
 * @brief kmalloc
 */
inline void *kmalloc(size_t size) {
    return kmalloc_flags(size, KMA_DEFAULT);
}


/**
 * @brief krealloc
 */
__attribute__((malloc)) void *krealloc(void *ptr, size_t size) {
    alloc_header_t *old_h = ALLOC_GETHEADER(ptr);
    // assert(old_h->magic == ALLOC_MAGIC_ALLOCATED);

    // size_t min_size = (old_h->alloc_size > size) ? size : old_h->alloc_size;

    // size = size + sizeof(alloc_header_t);

    // slab_cache_t *old_cache = alloc_getCache(old_h->cache_size);
    // slab_cache_t *new_cache = alloc_getCache(size);

    // // same cache?
    // if (old_cache && old_cache == new_cache) {
    //     // Same cache
    //     return ptr;
    // }

    // // Otherwise alloacte a new thing
    // void *new = NULL;
    // if (new_cache) {
    //     new = slab_allocate(new_cache);
    //     if (!new) return NULL;

    //     alloc_header_t *h = (alloc_header_t*)new;
    //     #ifdef ENABLE_TRACKING
    //         h->function = (uintptr_t)__builtin_return_address(0);
    //     #endif
    //     h->alloc_size = size - sizeof(alloc_header_t);
    // } else {
    //     new = vmm_map(NULL, size, VM_FLAG_ALLOC | VM_FLAG_DEFAULT, MMU_FLAG_PRESENT | MMU_FLAG_RW);
    //     if (!new) return NULL;


    //     alloc_header_t *h = (alloc_header_t*)new;
    //     h->cache_size = size;
    //     h->magic = ALLOC_MAGIC_ALLOCATED;
    //     h->alloc_size = size - sizeof(alloc_header_t);

    // #ifdef ENABLE_TRACKING
    //     h->function = (uintptr_t)__builtin_return_address(0);
    // #endif
    // }

    // memcpy((void*)((uintptr_t)new + sizeof(alloc_header_t)), ptr, min_size);
    
    // alloc_in_use -= old_h->alloc_size;
    // alloc_in_use += (size - sizeof(alloc_header_t));

    // if (old_cache) {
    //     slab_free(old_cache, old_h);
    // } else {
    //     vmm_unmap(old_h, old_h->cache_size);
    // }

    // return (void*)((uintptr_t)new + sizeof(alloc_header_t));

    void *p = kmalloc(size);
    memcpy(p, ptr, (old_h->alloc_size > size) ? size : old_h->alloc_size);
    kfree(ptr);
    return p;
}

/**
 * @brief kcalloc
 */
__attribute__((malloc)) void *kcalloc(size_t nobj, size_t size) {
    void *m = kmalloc(nobj * size);
    memset(m, 0, nobj * size);
    return m;
}

/**
 * @brief kfree
 */
void kfree(void *ptr) {
    // Get the allocation header
    alloc_header_t *h = ALLOC_GETHEADER(ptr);
    assert(h->magic == ALLOC_MAGIC_ALLOCATED);
    alloc_in_use -= h->alloc_size;
    
#ifdef ENABLE_TRACKING
    h->function = (uintptr_t)__builtin_return_address(0);
#endif

    slab_cache_t *c = alloc_getCache(h->cache_size);
    if (!c) {
        // This is a large allocation
        h->magic = ALLOC_MAGIC_FREE;
        vmm_unmap(h, h->cache_size);
    } else {
        slab_free(c, h);
    }
    

}

/**
 * @brief Initialize the memory allocator
 */
void alloc_init() {
    LOG(INFO, "Allocator initializing with %d caches\n", ALLOC_CACHES);

    for (unsigned i = 0; i < ALLOC_CACHES; i++) {
        alloc_caches[i] = slab_createCache("kmalloc cache", __alloc_cache_sizes[i], 0, __kmalloc_initializer, __kmalloc_deinitializer);
        assert(alloc_caches[i]);
    }
}

/**
 * @brief Get allocator bytes in use (cache)
 */
size_t alloc_used() {
    return alloc_in_use;
}

/**
 * @brief Print allocator statistics
 */
void alloc_stats() {
    LOG(DEBUG, "Allocator using %d bytes of memory\n", alloc_used());

    LOG(DEBUG, "Beginning cache dump:\n");

    for (int i = 0; i < ALLOC_CACHES; i++) {
        LOG(DEBUG, "%d size cache is using %d kB\n", alloc_caches[i]->slab_object_size, alloc_caches[i]->mem_usage / 1000);
    }
}

/**
 * @brief Post-SMP allocator hook
 */
void alloc_postSMPInit() {
    for (int i = 0; i < ALLOC_CACHES; i++) {
        slab_reinitializeCache(alloc_caches[i]);
    }
}