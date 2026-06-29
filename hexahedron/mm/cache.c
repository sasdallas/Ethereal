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
#include <kernel/task/process.h>
#include <kernel/init.h>
#include <string.h>
#include <stdatomic.h>

/* slabs */
slab_cache_t *page_cache_cache = NULL;
slab_cache_t *page_entry_cache = NULL;

/* amount of currently active pages in the system */
int pages_active = 0;

/* list of all page caches (for pruner) */
page_cache_t *cache_list = NULL;
mutex_t cache_list_lock = MUTEX_INITIALIZER;

/* current system period */
char cache_period = 0;

/* sync event */
static page_entry_t *dirty_list = NULL;
static mutex_t dirty_lock = MUTEX_INITIALIZER; 
static event_t sync_event;

/* Log method */
#define LOG(status, ...) dprintf_module(status, "MM:CACHE", __VA_ARGS__)

/**
 * @brief Initialize page cache
 */
void cache_init() {
    EVENT_INIT(&sync_event);
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

    // add it to the cache list
    mutex_acquire(&cache_list_lock);
    cache->next = cache_list;
    cache->prev = NULL;
    if (cache_list) cache_list->prev = cache;
    cache_list = cache;
    mutex_release(&cache_list_lock);

    return cache;
}

/**
 * @brief Internal method to retrieve a page from the cache
 * Expects lock held
 */
static pmm_page_t *cache_get(page_cache_t *cache, loff_t offset) {
    size_t idx = offset / PAGE_SIZE;
    page_entry_t *ent = cache->page_map[idx % PAGE_CACHE_MAP_ENTS];
    while (ent) {
        if (ent->page->offset == offset) {
            ent->period = __atomic_load_n(&cache_period, __ATOMIC_SEQ_CST); // see cache_pruner thread
            
            if (ent->being_evicted) {
                // We happened to get a page at a time when it was just about to be evicted.
                // This indicates a deferred sync
                // Right now we hold the lock so we can just clear this flag
                // TODO update this algorithm since a program can just access a page to stop it from getting evicted
                ent->being_evicted = false;
            }
            
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
    __atomic_add_fetch(&pages_active, 1, __ATOMIC_SEQ_CST);
    entry->page = page;
    page->ent = entry;
    page->offset = offset;
    page->inode = inode;
    entry->period = __atomic_load_n(&cache_period, __ATOMIC_SEQ_CST);
    
    size_t idx = (offset / PAGE_SIZE);
    entry->next = inode->cache->page_map[idx % PAGE_CACHE_MAP_ENTS];
    inode->cache->page_map[idx % PAGE_CACHE_MAP_ENTS] = entry;
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
        EVENT_DETACH(&l);
        EVENT_DESTROY_LISTENER(&l);
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
    size_t idx = page->offset / PAGE_SIZE;
    page_entry_t *ent = cache->page_map[idx % PAGE_CACHE_MAP_ENTS];
    page_entry_t *prev = NULL;
    __atomic_sub_fetch(&pages_active, 1, __ATOMIC_SEQ_CST);
    while (ent) {
        if (ent->page == page) {
            if (prev) {
                prev->next = ent->next;
            } else {
                cache->page_map[idx % PAGE_CACHE_MAP_ENTS] = ent->next;
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
 * @param inode The VFS inode to retrieve
 * @param offset The offset in the file to retrieve a page (page-aligned)
 * @param output The output page address
 * @returns 0 on success
 */
int cache_getPage(struct vfs_inode *inode, loff_t offset, pmm_page_t **output) {
    page_cache_t *cache = inode->cache;

    mutex_acquire(&cache->lck);
    pmm_page_t *page = cache_get(cache, offset); // updates entry period
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
            pmm_retainPage(page);
            mutex_release(&cache->lck);
            *output = page;
            return 0;
        } else if (page->flags & PAGE_FLAG_ERROR) {
            // An error occurred while trying to take this page
            cache_remove(cache, page);
            pmm_releasePage(page);
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
    int ret = cache_place(inode, offset, page); // automatically updates page period
    mutex_release(&cache->lck);
    if (ret < 0) {
        pmm_releasePage(page);
        return ret;
    } 

    // Perform I/O now
    uintptr_t pg_mapped = arch_mmu_remap_physical(pg, PAGE_SIZE, REMAP_TEMPORARY);
    int r = inode->c_ops->read_page(inode, offset, pg_mapped);
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
    pmm_retainPage(page);
    mutex_release(&cache->lck);
    *output = page;
    return 0;
}

/**
 * @brief Destroy and free an inode cache
 */
void cache_destroy(vfs_inode_t *inode) {
    page_cache_t *c = inode->cache;
    if (!c) return;
    
    dprintf(DEBUG, "cache_destroy on inode %p\n", inode);

    // !!! acquiring this lock will stop the pruner thread from messing everything up
    mutex_acquire(&cache_list_lock);

    // when ready, we destroy all pages
    for (int i = 0; i < PAGE_CACHE_MAP_ENTS; i++) {
        page_entry_t *ent = c->page_map[i];
        while (ent) {
            page_entry_t *nxt = ent->next;
            
            if (ent->page->flags & PAGE_FLAG_DIRTY) {
                // Damn this is a dirty page, we can't free it here
                LOG(WARN, "Dirty page %p cannot be freed yet\n", ent->page);
                ent->being_evicted = true;
                ent->page->flags |= PAGE_FLAG_TRUNCATED;
                ent = nxt;
                continue;
            }

            cache_remove(c, ent->page); // !!! this re-searches the entire list 
            pmm_releasePage((ent->page));
            slab_free(page_entry_cache, ent);
            ent = nxt;
        }
    }

    // Now update the cache list
    if (c == cache_list) cache_list = c->next;
    if (c->next) c->next->prev = c->prev;
    if (c->prev) c->prev->next = c->next;
    c->next = c->prev = NULL;

    mutex_release(&cache_list_lock);
    slab_free(page_cache_cache, c);
}

/**
 * @brief Mark a page as dirty (mappings with mmap will always mark as dirty)
 * @param page The page to mark as dirty
 */
void cache_markDirty(pmm_page_t *page) {
    if (page->flags & PAGE_FLAG_PERMANENT) return; // permanent pages like those in tmpfs do not need syncing

    pmm_retainPage(page); // will be dropped by syncer
    
    if (page->flags & PAGE_FLAG_DIRTY) {
        // already dirty
        pmm_releasePage(page);
        return;
    }

    page->flags |= PAGE_FLAG_DIRTY;

    // add to dirty list
    mutex_acquire(&dirty_lock);
    page->ent->dirty_next = dirty_list;
    dirty_list = page->ent;
    mutex_release(&dirty_lock);

    return;
}

/**
 * @brief Evict while cache is locked
 */
static void cache_evictLocked(pmm_page_t *page) {
    if (page->flags & PAGE_FLAG_DIRTY) {
        LOG(WARN, "Evicting dirty page! Trying to compensate...\n");
        page->ent->being_evicted = true;
        return;
    }

    // remove the page from cache as it is clean
    page_cache_t *c = page->inode->cache;
    cache_remove(c, page);

    // release the page
    pmm_releasePage(page);
}

/**
 * @brief Evict a page from the cache
 * @param page The page to evict from the cache
 */
void cache_evict(pmm_page_t *page) {
    page_cache_t *c = page->inode->cache;
    mutex_acquire(&c->lck);
    cache_evictLocked(page);
    mutex_release(&c->lck);
}

/**
 * @brief Truncate a cache for an inode
 * @param inode The inode cache to truncate
 * @param new_size The new size of the cache
 */
void cache_truncate(vfs_inode_t *inode, loff_t new_size) {
    if (new_size >= inode->attr.size || (inode->attr.size-new_size) < PAGE_SIZE) {
        return; // we only care when pages are lost
    }

    page_cache_t *c = inode->cache;
    mutex_acquire(&c->lck);

    loff_t start_loss = PAGE_ALIGN_UP(new_size);
    for (int i = (start_loss/PAGE_SIZE) % PAGE_CACHE_MAP_ENTS; i < PAGE_CACHE_MAP_ENTS; i++) {
        page_entry_t *ent = c->page_map[i];
        while (ent) {
            page_entry_t *nxt = ent->next;
            
            if (ent->page->offset >= start_loss) {
                // This one needs to be evicted
                cache_evictLocked(ent->page);
            }

            ent = nxt;
        }
    }

    mutex_release(&c->lck);
}

/**
 * @brief Get active pages
 */
int cache_active() {
    return __atomic_load_n(&pages_active, __ATOMIC_SEQ_CST);
}

/**
 * @brief Cache syncer thread
 */
void cache_syncer(void *arg) {
    for (;;) {
        event_listener_t l;
        EVENT_INIT_LISTENER(&l);
        EVENT_ATTACH(&l, &sync_event);

        // get the queue
        mutex_acquire(&dirty_lock);
        page_entry_t *dirty = dirty_list;
        dirty_list = NULL;
        mutex_release(&dirty_lock);
        
        // now flush the existing queue
        while (dirty) {
            page_entry_t *nxt = dirty->dirty_next;
            pmm_page_t *pg = dirty->page;
            page_cache_t *c = pg->inode->cache; 
            
            if (dirty->page->flags & PAGE_FLAG_TRUNCATED) {
                // This page was truncated while dirty, we can release our reference and free the entry
                pmm_releasePage(dirty->page);
                slab_free(page_entry_cache, dirty);
                goto _next_page;
            }
            

            // lock cache
            mutex_acquire(&c->lck);

            // mark the page as loading, and lose the lock so it can be waited on
            dirty->page->flags &= ~(PAGE_FLAG_DIRTY | PAGE_FLAG_READY | PAGE_FLAG_ERROR);
            dirty->page->flags |= PAGE_FLAG_LOADING;
            
            // Sync the page to disk
            mutex_release(&c->lck);

            // Sync the page to disk while unlocked
            // Anyone who tries to getPage will automatically clear the being_evicted flag
            uintptr_t pg_mapped = arch_mmu_remap_physical(pmm_address(pg), PAGE_SIZE, REMAP_TEMPORARY);
            int r = pg->inode->c_ops->write_page(pg->inode, pg->offset, pg_mapped);
            arch_mmu_unmap_physical(pg_mapped, PAGE_SIZE);
            
            mutex_acquire(&c->lck);

            if (r < 0) {
                LOG(ERR, "Failed to sync page for inode %p (off=%d) due to error %d\n", pg->inode, pg->offset, r);
                cache_pageLoadFinished(c, pg, PAGE_FLAG_ERROR);
            } else {
                cache_pageLoadFinished(c, pg, PAGE_FLAG_READY);
            }

            // Now lose the reference that cache_markDirty added
            pmm_releasePage((pg));

            if (dirty->being_evicted) {
                // We are being evicted still
                cache_remove(c, pg);
                pmm_releasePage((pg));
                slab_free(page_entry_cache, dirty);
            }

            mutex_release(&c->lck);
        
        _next_page:
            dirty = nxt;
        }
    
        EVENT_WAIT(&l, -1);
        EVENT_DETACH(&l);
        EVENT_DESTROY_LISTENER(&l);
    }
}

/**
 * @brief Cache pruner thread
 */
void cache_pruner(void *arg) {
    // The way this pruner works:
    // cache_period can be 0 or 1 and on getPage the returned page will have its entry's period
    // set to cache_period. This pruner sleeps for an interval, flips the periods, and checks
    // for any pages on the new period (as they have missed two rounds). If it finds them they are pruned.
    for (;;) {
        sleep_time(15, 0); // every 15s
        sleep_enter();

        unsigned char next_period = __atomic_load_n(&cache_period, __ATOMIC_SEQ_CST) ^ 1;

        // !!! This is an O(n) algorithm, should maybe add some type of LRU to it?
        // !!! This code is extremely slow for cache standards...
        mutex_acquire(&cache_list_lock);

        bool wakeup_syncer = false;
        page_cache_t *iter = cache_list;
        while (iter) {
            if (mutex_tryAcquire(&iter->lck) == 0) {
                // This cache is highly active
                goto _next_cache;
            }
            
            for (int i = 0; i < PAGE_CACHE_MAP_ENTS; i++) {
                page_entry_t *ent = iter->page_map[i];
                page_entry_t *prev = NULL;

                while (ent) {
                    page_entry_t *nxt = ent->next;
                    pmm_page_t *pg = ent->page;
                    uintptr_t page_addr = pmm_address(pg);

                    // permanent pages cannot be evicted (like those used in the tmpfs)
                    if (ent->period == next_period && ((pg->flags & PAGE_FLAG_PERMANENT) == 0)) {
                        // This page needs to be evicted
                        dprintf(DEBUG, "Evicting page: %p\n", page_addr);
                        
                        if (pg->flags & PAGE_FLAG_DIRTY) {
                            // We CANT flush this page yet since we are holding a mutex and that would block too long!
                            // Defer to syncer!!
                            ent->being_evicted = true;
                            wakeup_syncer = true;
                            goto _next_page;
                        }
                        
                        // cache_remove would rewalk the entire list
                        if (ent == iter->page_map[i]) {
                            iter->page_map[i] = ent->next;
                        } else {
                            prev->next = ent->next;
                        }

                        __atomic_sub_fetch(&pages_active, 1, __ATOMIC_SEQ_CST);
                        pmm_release(page_addr);
                        slab_free(page_entry_cache, ent);
                    }
                    
                _next_page:
                    prev = ent;
                    ent = nxt;
                }
            }
            mutex_release(&iter->lck);

        _next_cache:
            iter = iter->next;
        }
    
        __atomic_store_n(&cache_period, next_period, __ATOMIC_SEQ_CST);
        mutex_release(&cache_list_lock);

        if (wakeup_syncer) {
            EVENT_SIGNAL(&sync_event);
        }
    }
}

/**
 * @brief Initialize cache pruner
 */
int cache_prunerInit() {
    process_t *cache_pruner_proc = process_createKernel("cache pruner", PROCESS_KERNEL, PRIORITY_MED, cache_pruner, NULL);
    scheduler_insertThread(cache_pruner_proc->main_thread);
    process_t *cache_syncer_proc = process_createKernel("cache syncer", PROCESS_KERNEL, PRIORITY_MED, cache_syncer, NULL);
    scheduler_insertThread(cache_syncer_proc->main_thread);
    return 0;
}

SCHED_INIT_ROUTINE(cache, INIT_FLAG_DEFAULT, cache_prunerInit);
