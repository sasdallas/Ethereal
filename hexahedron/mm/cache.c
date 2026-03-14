/**
 * @file hexahedron/mm/cache.c
 * @brief The page cache for Hexahedron
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/fs/vfs_new.h>
#include <kernel/mm/cache.h>
#include <kernel/mm/vmm.h>
#include <kernel/debug.h>
#include <structs/hashmap.h>
#include <kernel/processor_data.h>
#include <string.h>
#include <stdatomic.h>

slab_cache_t *page_cache_cache = NULL;
slab_cache_t *page_entry_cache = NULL;

/**
 * @brief Initialize page cache
 */
void cache_init() {
    assert((page_cache_cache = slab_createCache("page cache", SLAB_CACHE_DEFAULT, sizeof(page_cache_t), 0, NULL, NULL)));
    assert((page_entry_cache = slab_createCache("page entry", SLAB_CACHE_DEFAULT, sizeof(page_entry_t), 0, NULL, NULL)));
}

/**
 * @brief Initialize cache for inode
 */
page_cache_t* cache_create() {
    page_cache_t *cache = slab_allocate(page_cache_cache);
    assert(cache);
    memset(cache->page_map, 0, sizeof(page_cache_t*) * PAGE_CACHE_MAP_ENTS);
    MUTEX_INIT(&cache->lck);
    EVENT_INIT(&cache->ready_event);
    SPINLOCK_INIT(&cache->ready_event_lock);
    return cache;
}

/**
 * @brief Internal method to retrieve a page from the cache
 * Expects lock held
 */
static pmm_page_t *cache_get(page_cache_t *cache, loff_t offset) {
    page_entry_t *ent = cache->page_map[offset % PAGE_CACHE_MAP_ENTS];
    while (ent) {
        if (ent->page->offset == offset) {
            return ent->page;
        }
        ent = ent->next;
    }

    return NULL;
}

/**
 * @brief Place page into cache
 * Expects lock held and page referenced already
 */
static int cache_place(vfs_inode_t *inode, loff_t offset, pmm_page_t *page) {
    page_entry_t *entry = slab_allocate(page_entry_cache);
    if (!entry) return -ENOMEM;
    entry->page = page;
    page->offset = offset;
    page->inode = inode;
    entry->period = 0; // TODO
    entry->next = inode->cache->page_map[offset % PAGE_CACHE_MAP_ENTS];
    inode->cache->page_map[offset % PAGE_CACHE_MAP_ENTS] = entry;
    return 0;
}

/**
 * @brief Wait until a page is done loading
 * Cache must be locked already
 */
static int cache_wait(page_cache_t *cache, pmm_page_t *page) {
    spinlock_acquire(&cache->ready_event_lock);
    while (page->flags & PAGE_FLAG_LOADING) {
        event_listener_t l;
        EVENT_INIT_LISTENER(&l);
        EVENT_ATTACH(&l, &cache->ready_event);
        spinlock_release(&cache->ready_event_lock);
        mutex_release(&cache->lck);
        int r = EVENT_WAIT(&l, -1);
        mutex_acquire(&cache->lck);
        if (r < 0) return r;
        spinlock_acquire(&cache->ready_event_lock);
    }

    spinlock_release(&cache->ready_event_lock);
    return 0;
}

/**
 * @brief Remove page from the cache list
 */
static void cache_remove(page_cache_t *cache, pmm_page_t *page) {
    page_entry_t *ent = cache->page_map[page->offset % PAGE_CACHE_MAP_ENTS];
    page_entry_t *prev = NULL;
    while (ent) {
        if (ent->page == page) {
            if (prev) {
                prev->next = ent->next;
            } else {
                cache->page_map[page->offset % PAGE_CACHE_MAP_ENTS] = ent->next;
            }

            return;
        }

        prev = ent;
        ent = ent->next;
    }

    assert(0);
}

/**
 * @brief Cache finished loading page
 */
static void cache_pageLoadFinished(page_cache_t *cache, pmm_page_t *page, int final_state) {
    spinlock_acquire(&cache->ready_event_lock);
    page->flags &= ~PAGE_FLAG_LOADING;
    page->flags |= final_state;
    EVENT_SIGNAL(&cache->ready_event);
    spinlock_release(&cache->ready_event_lock);
}

/**
 * @brief Get a page from the page cache
 * @param node The VFS node to retrieve
 * @param offset The offset in the file to retrieve a page (page-aligned)
 * @param output The output page address
 * @returns 0 on success
 */
int cache_getPage(struct vfs_file *node, loff_t offset, pmm_page_t **output) {
    page_cache_t *cache = node->inode->cache;

    mutex_acquire(&cache->lck);
    pmm_page_t *page = cache_get(cache, offset);
    if (page) {
        // already got the page, but is it ready?
        if (page->flags & PAGE_FLAG_LOADING) {
            int ret = cache_wait(cache, page);
            if (ret < 0) {
                mutex_release(&cache->lck);
                return ret;
            }
        }

        // Now what's going on with this page
        if (page->flags & PAGE_FLAG_READY) {
            pmm_retain(pmm_address(page));
            mutex_release(&cache->lck);
            *output = page;
            return 0;
        } else if (page->flags & PAGE_FLAG_ERROR) {
            // An error occurred while trying to take this page
            cache_remove(cache, page);
            pmm_release(pmm_address(page));
            mutex_release(&cache->lck);
            return -EIO; // TODO: better error code passing
        } else {
            assert(0);
        }
    }

    // allocate a new page
    uintptr_t pg = pmm_allocatePage(ZONE_DEFAULT);
    page = pmm_page(pg);

    // place the page in cache marked as loading. anything that gets it will stop until ready
    page->flags |= PAGE_FLAG_LOADING;
    int ret = cache_place(node->inode, offset, page);
    mutex_release(&cache->lck);
    if (ret < 0) {
        return ret;
    } 

    // Perform I/O now
    uintptr_t pg_mapped = arch_mmu_remap_physical(pg, PAGE_SIZE, REMAP_TEMPORARY);
    int r = node->inode->c_ops->read_page(node, offset, pg_mapped);
    arch_mmu_unmap_physical(pg_mapped, PAGE_SIZE);
    mutex_acquire(&cache->lck);

    if (r < 0) {
        // Damn, next call that gets this page (or async evictor) will drop it.
        cache_pageLoadFinished(cache, page, PAGE_FLAG_ERROR);
        mutex_release(&cache->lck);
        return r;
    }

    // I/O succeeded
    cache_pageLoadFinished(cache, page, PAGE_FLAG_READY);
    mutex_release(&cache->lck);
    pmm_retain(pmm_address(page));
    *output = page;
    return 0;
}

/**
 * @brief Mark a page as dirty (mappings with mmap will always mark as dirty)
 * @param page The page to mark as dirty
 */
void cache_markDirty(pmm_page_t *page) {
    return;
}

/**
 * @brief Evict a page from the cache
 * @param page The page to evict from the cache
 */
void cache_evict(pmm_page_t *page) {
    return;
}