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
#include <stdlib.h>

/* slabs */
slab_cache_t *page_cache_cache = NULL;
slab_cache_t *page_entry_cache = NULL;

/* statistics */
int pages_active = 0;
int pages_dirty = 0;

/* list of all page caches */
static DLIST_HEAD(cache_list, page_cache_t);
mutex_t cache_list_lock = MUTEX_INITIALIZER;

/* current system period */
char cache_period = 0;

/* sync event */
static mutex_t dirty_lock = MUTEX_INITIALIZER; 
static mutex_t sync_lock = MUTEX_INITIALIZER;
static page_cache_t *dirty_cache_list = NULL;
static event_t sync_event;

static DLIST_HEAD(dirty_list, page_cache_t);

/* max pages per range in sync */
#define CACHE_SYNC_MAX_PAGES 128

/* Log method */
#define LOG(status, ...) dprintf_module(status, "MM:CACHE", __VA_ARGS__)

/* atomicity helpers */
#define PAGE_TEST_FLAG(pg, flag)  (__atomic_load_n(&(pg)->flags, __ATOMIC_SEQ_CST) & flag)
#define PAGE_IS_LOADING(pg) PAGE_TEST_FLAG(pg, PAGE_FLAG_LOADING)
#define PAGE_IS_READY(pg) PAGE_TEST_FLAG(pg, PAGE_FLAG_READY)
#define PAGE_IS_ERROR(pg) PAGE_TEST_FLAG(pg, PAGE_FLAG_ERROR)
#define PAGE_IS_DIRTY(pg) PAGE_TEST_FLAG(pg, PAGE_FLAG_DIRTY)
#define PAGE_IS_WRITEBACK(pg) PAGE_TEST_FLAG(pg, PAGE_FLAG_WRITEBACK)
#define PAGE_IS_TRUNCATED(pg) PAGE_TEST_FLAG(pg, PAGE_FLAG_TRUNCATED)
#define PAGE_MARK_DIRTY(pg) (__atomic_fetch_or(&(pg)->flags, PAGE_FLAG_DIRTY, __ATOMIC_SEQ_CST) & PAGE_FLAG_DIRTY)
#define PAGE_MARK_TRUNC(pg) (__atomic_fetch_or(&(pg)->flags, PAGE_FLAG_TRUNCATED, __ATOMIC_SEQ_CST))
#define PAGE_MARK_WRITEBACK(pg) (__atomic_fetch_or(&(pg)->flags, PAGE_FLAG_WRITEBACK, __ATOMIC_SEQ_CST))
#define PAGE_CLEAR_WRITEBACK(pg) (__atomic_fetch_and(&(pg)->flags, ~(PAGE_FLAG_WRITEBACK), __ATOMIC_SEQ_CST))

#define __PAGE_DO_CAS(pg, tomask, toset) ({ \
                                            uint32_t saved_fl = __atomic_load_n(&(pg)->flags, __ATOMIC_SEQ_CST);\
                                            uint32_t new_fl;\
                                            do {\
                                                new_fl = (saved_fl & ~((tomask))) | (toset);\
                                            } while (!__atomic_compare_exchange_n(&(pg)->flags, &saved_fl, new_fl, true, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST));\
                                        })
                                    
#define PAGE_MARK_STATE(pg,state) __PAGE_DO_CAS(pg, PAGE_FLAG_LOADING, state)
#define PAGE_MARK_LOADING(pg) __PAGE_DO_CAS(pg, PAGE_FLAG_READY | PAGE_FLAG_ERROR, PAGE_FLAG_LOADING)


/**
 * @brief Initialize page cache
 */
void cache_init() {
    EVENT_INIT(&sync_event);
    DLIST_INIT(&dirty_list);
    DLIST_INIT(&cache_list);
    assert((page_cache_cache = slab_createCache("page cache", SLAB_CACHE_DEFAULT, sizeof(page_cache_t), 0, NULL, NULL)));
    assert((page_entry_cache = slab_createCache("page entry", SLAB_CACHE_DEFAULT, sizeof(page_entry_t), 0, NULL, NULL)));
}

/**
 * @brief Initialize cache for inode
 */
page_cache_t* cache_create() {
    page_cache_t *cache = slab_allocate(page_cache_cache);
    memset(cache, 0, sizeof(page_cache_t));
    assert(cache);
    XA_INIT(&cache->xa);
    RWSEM_INIT(&cache->sem);
    EVENT_INIT(&cache->ready_event);
    SPINLOCK_INIT(&cache->ready_event_lock);
    MUTEX_INIT(&cache->dirty.sync_lock);
    memset(cache->dirty.dirty, 0, sizeof(cache->dirty.dirty));
    cache->dirty.is_dirty = false;
    cache->dirty.on_dirty_list = false;

    // add it to the cache list
    mutex_acquire(&cache_list_lock);
    DLIST_INSERT_TAIL(&cache_list, cache, node);
    mutex_release(&cache_list_lock);
    return cache;
}

/**
 * @brief Internal method to retrieve a page from the cache
 * Expects lock held
 */
static pmm_page_t *cache_get(page_cache_t *cache, loff_t offset) {
    size_t idx = offset / PAGE_SIZE;
    
    page_entry_t *ent = xa_load(&cache->xa, idx);
    if (ent) {
        if (ent->being_evicted) {
            // We happened to get a page at a time when it was just about to be evicted.
            // This indicates a deferred sync
            // Right now we hold the lock so we can just clear this flag
            // TODO does this work???
            ent->being_evicted = false;
        }

        if (ent->page->offset != offset) {
            LOG(ERR, "There is corruption in the page array at index %d (trying to get offset %d).\n", idx, offset);
            LOG(ERR, "Dumping all pages.\n");

            unsigned long index;
            page_entry_t *e;
            xa_foreach(&cache->xa, index, e) {
                LOG(ERR, "Index %d: Page %p/%p (page->offset = %d, page->inode = %p)\n", index, e, e->page, e->page->offset, e->page->inode);
            }

            assert(0);
        }

        return ent->page;
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
    entry->in_dirty_list = false;
    entry->being_evicted = false;
    memset(entry->dirty, 0, sizeof(entry->dirty));
    page->ent = entry;
    page->offset = offset;
    page->inode = inode;
    entry->period = __atomic_load_n(&cache_period, __ATOMIC_SEQ_CST);

    size_t idx = (offset / PAGE_SIZE);
    xa_store(&inode->cache->xa, idx, entry);

    return 0;
}

/**
 * @brief Wait until a page is done loading
 * Cache must be locked already
 */
static int cache_wait(page_cache_t *cache, pmm_page_t *page) {
    CACHE_FINISH_READ(cache);
    int r = 0;
    while (PAGE_IS_LOADING(page)) {
        event_listener_t l;
        EVENT_INIT_LISTENER(&l);
        EVENT_ATTACH(&l, &cache->ready_event);

        if (!PAGE_IS_LOADING(page)) {
            EVENT_DETACH(&l);
            EVENT_DESTROY_LISTENER(&l);
            CACHE_START_READ(cache);
            return 0;
        }

        r = EVENT_WAIT(&l, -1);
 
        EVENT_DETACH(&l);
        EVENT_DESTROY_LISTENER(&l);
        if (r < 0) break;
    }

    CACHE_START_READ(cache);
    return r;
}

/**
 * @brief Remove page from the cache list
 */
static void cache_remove(page_cache_t *cache, pmm_page_t *page) {
    size_t idx = page->offset / PAGE_SIZE;
    __atomic_sub_fetch(&pages_active, 1, __ATOMIC_SEQ_CST);
    xa_erase(&cache->xa, idx);
}

/**
 * @brief Cache finished loading page
 */
static void cache_pageLoadFinished(page_cache_t *cache, pmm_page_t *page, int final_state) {
    PAGE_MARK_STATE(page, final_state);
    EVENT_SIGNAL(&cache->ready_event);
}

/**
 * @brief Cache finished loading a bunch of pages
 */
static void cache_pageRangeFinished(page_cache_t *cache, page_range_t *range, int final_state) {
    for (unsigned i = 0; i < range->npages; i++) {
        if (range->pages[i]) {
            pmm_page_t *pg = pmm_page(range->pages[i]);
            assert(pg->ent->being_evicted == false);
            PAGE_MARK_STATE(pg, final_state);
        }
    }
    EVENT_SIGNAL(&cache->ready_event);
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

    CACHE_START_READ(cache);

    pmm_page_t *page = cache_get(cache, offset); // updates entry period
    if (page) {
        // already got the page, but is it ready?
        if (PAGE_IS_LOADING(page)) {
            int ret = cache_wait(cache, page);
            if (ret < 0) {
                CACHE_FINISH_READ(cache);
                return ret;
            }
        }

        // Now what's going on with this page
        if (PAGE_IS_READY(page)) {
            pmm_retainPage(page);
            CACHE_FINISH_READ(cache);
            *output = page;
            return 0;
        } else if (PAGE_IS_ERROR(page)) {
            // An error occurred while trying to take this page
            // Annoying but we need the write lock to do this
            CACHE_FINISH_READ(cache);

            CACHE_START_WRITE(cache);

            // !!! Is this check necessary?
            if (cache_get(cache, offset)) {
                cache_remove(cache, page);
            }
            
            CACHE_FINISH_WRITE(cache);

            pmm_releasePage(page);
            return -EIO; // TODO: better error code passing
        } else {
            assert(0);
        }
    }

    CACHE_FINISH_READ(cache);

    // allocate a new page
    uintptr_t pg = pmm_allocatePage(ZONE_DEFAULT);
    page = pmm_page(pg);

    // place the page in cache marked as loading. anything that gets it will stop until ready
    page->flags |= PAGE_FLAG_LOADING;

    CACHE_START_WRITE(cache);
    
    // we might have lost this race
    if (cache_get(cache, offset) != NULL) {
        // !!! I don't really like this since it can technically infinite recurse
        // !!! To be fixed
        CACHE_FINISH_WRITE(cache);
        pmm_releasePage(page);
        return cache_getPage(inode,offset,output);
    }

    // now we can add this page into the cache as a loading page
    int ret = cache_place(inode, offset, page); // automatically updates page period

    // ref this page if possible
    if (ret == 0) {
        pmm_retainPage(page);
    }

    // done writing, others can sleep now
    CACHE_FINISH_WRITE(cache);
    if (ret < 0) {
        pmm_releasePage(page);
        return ret;
    } 

    // Perform I/O now
    CACHE_DEFINE_PAGE_RANGE(range, 1);
    range->offset = offset;
    range->pages[0] = pmm_address(page);
    range->npages = 1;
    int r = inode->c_ops->read_range(inode, range);
    
    // State cleanup 
    CACHE_START_WRITE(cache);

    if (r < 0) {
        cache_pageLoadFinished(cache, page, PAGE_FLAG_ERROR);
        CACHE_FINISH_WRITE(cache);
        return r;
    }

    cache_pageLoadFinished(cache, page, PAGE_FLAG_READY);
    CACHE_FINISH_WRITE(cache);

    *output = page;
    return 0;
}

/**
 * @brief Get a page range
 * @param inode The inode to get the page range for
 * @param offset The offset in the inode to retrieve a range
 * @param npages The number of pages to receive
 * @param range The range to output to
 * @param flags Flags
 * @returns 0 on success
 */
int cache_getRange(struct vfs_inode *inode, loff_t offset, size_t npages, page_range_t *range, int flags) {
    page_cache_t *cache = inode->cache;
    size_t filled = 0;
    range->offset = offset;
    range->npages = npages;
    memset(range->pages, 0, npages * sizeof(uintptr_t));

    bool have_dirty = false;

    // Whatever we don't fill will be caught in the write section
    CACHE_START_READ(cache);
    loff_t this_off = offset;
    for (unsigned i = 0; i < npages; i++) {
        // Try to get the page here, then handle based on what's happening
        pmm_page_t *pg = cache_get(cache, this_off);
        if (pg) {
            if (PAGE_IS_LOADING(pg)) {
                // !!! avoid having to sleep here maybe just re-read it kek
                int ret = cache_wait(cache, pg);
                if (ret < 0) {
                    CACHE_FINISH_READ(cache);
                    return ret;
                }
            }
            
            if (PAGE_IS_READY(pg)) {
                // We successfully filled a page
                if (PAGE_IS_DIRTY(pg)) {
                    // Dirty pages cannot be batched, and so missing pages have to be read
                    have_dirty = true;
                }

                filled++;
                pmm_retainPage(pg);
                range->pages[i] = pmm_address(pg);
            } else if (PAGE_IS_ERROR(pg)) {
                CACHE_FINISH_READ(cache);
                CACHE_START_WRITE(cache);

                if (cache_get(cache, offset)) {
                    cache_remove(cache, pg);
                }
                
                CACHE_FINISH_WRITE(cache);
                pmm_releasePage(pg);
                return -EIO; // TODO: better error code passing
            } else {
                assert(0);
            }
        }

        this_off += PAGE_SIZE;
    }
    CACHE_FINISH_READ(cache);

    if (filled == npages) {
        return 0;
    }

    // Optimizing page-aligned writes
    // These don't need to be read, so if we didn't get them allocate some dummy memory and use that
    if (flags & CACHE_FLAG_ALIGNEDWRITE) {
        CACHE_START_WRITE(cache);
        
        this_off = offset;
        for (unsigned i = 0; i < npages; i++) {
            if (range->pages[i] == 0x0) {
                pmm_page_t *pg = cache_get(cache, this_off);
                if (pg) {
                    // todo im lazy
                    assert(pg->flags & PAGE_FLAG_READY && "alignedwrite waiting not impl'd");
                    pmm_retainPage(pg);
                    range->pages[i] = pmm_address(pg);
                } else {
                    // allocate some dummy memory thatll be filled anyways
                    uintptr_t addr = pmm_allocatePage(ZONE_DEFAULT);
                    pg = pmm_page(addr);
                    pmm_retainPage(pg);
                    pg->flags |= PAGE_FLAG_READY; // TODO: what if a read comes in at this exact moment?
                    cache_place(inode, this_off, pg);
                    range->pages[i] = addr;
                }
            }

            this_off += PAGE_SIZE;
        }

        CACHE_FINISH_WRITE(cache);

        return 0;
    }

    // On certain disks it is way faster to drop a couple of pages in order to do a big sequential read
    // We should batch them if 50% pages were missing
    size_t missed = npages - filled;
    if (have_dirty || missed < npages/2 || !inode->c_ops->read_range) {
        // Read individual pages
        this_off = offset;
        for (unsigned i = 0; i < npages; i++) {
            if (range->pages[i] == 0x0) {
                pmm_page_t *pg;
                int r = cache_getPage(inode, this_off, &pg);
                if (r != 0) {
                    return r;
                }

                range->pages[i] = pmm_address(pg);
            }

            this_off += PAGE_SIZE;
        }
        
        return 0;
    } else {
        // Else, batch the entire thing
        CACHE_START_WRITE(cache);
        this_off = offset; 
        for (unsigned i = 0; i < npages; i++) {
            if (range->pages[i] != 0x0) {
                // This page could not have been evicted
                // Probably
                pmm_page_t *pg = pmm_page(range->pages[i]);
                PAGE_MARK_LOADING(pg);
                goto _batch_next;
            } else {
                // Make sure another thread didnt sneakily put one in
                pmm_page_t *pg = cache_get(cache, this_off);
                if (pg) {
                    pmm_retainPage(pg);
                    PAGE_MARK_LOADING(pg);
                    range->pages[i] = pmm_address(pg);
                    goto _batch_next;
                }
            }


            // Allocate a new page
            uintptr_t addr = pmm_allocatePage(ZONE_DEFAULT);
            pmm_page_t *pg = pmm_page(addr);
            pmm_retainPage(pg);
            pg->flags |= PAGE_FLAG_LOADING;
            cache_place(inode, this_off, pg);
            range->pages[i] = addr;

        _batch_next:
            this_off += PAGE_SIZE;
        }

        // all pages are batched and ready
        CACHE_FINISH_WRITE(cache);

        // begin!
        int r = inode->c_ops->read_range(inode, range);

        CACHE_START_WRITE(cache);
        
        if (r == 0) {
            // success!
            cache_pageRangeFinished(cache, range, PAGE_FLAG_READY);
        } else {
            // not success!
            LOG(ERR, "read_range failed with error code %d\n", r);
            cache_pageRangeFinished(cache, range, PAGE_FLAG_ERROR);
        }

        CACHE_FINISH_WRITE(cache);
        return r;
    }
}

/**
 * @brief Release a range of pages
 * @param range The range to release
 */
void cache_releaseRange(page_range_t *range) {
    for (unsigned i = 0; i < range->npages; i++) {
        pmm_release(range->pages[i]);
    }
}

/**
 * @brief Destroy an inode cache
 */
void cache_destroy(vfs_inode_t *inode) {
    page_cache_t *c = inode->cache;
    if (!c) return;
    
    dprintf(DEBUG, "cache_destroy on inode %p\n", inode);

    // !!! THIS FUNCTION IS RACY AND WILL PROBABLY CRASH.
    // !!! A few assertions should help cause a known crash but UAFs are possible with this.
    // !!! TO BE FIXED.

    // HACK: Sync the cache now to remove all the dirty pages
    cache_syncInode(inode);

    // !!!: this is a hack
    // basically the cache has no way of knowing whether the inode is actively being processed by the syncer thread
    // this is because i am bad at designing a dirty cache system
    // however this is fine because we have hacks, this just defers the syncer thread to process it next sync IF its not possible to pop it.
    CACHE_START_WRITE(c);
    
    // remove the cache from the dirty list
    mutex_acquire(&dirty_lock);
    if (c->dirty.on_dirty_list) {
        DLIST_REMOVE(&dirty_list, page_cache_t, c, dirty.node);
    }
    mutex_release(&dirty_lock);

    CACHE_FINISH_WRITE(c);

    assert(c->dirty.is_dirty == false && "cache_destroy on a dirty cache");

    // when ready, we destroy all pages
    unsigned long idx;
    void *ent;
    xa_foreach(&c->xa, idx, ent) {
        page_entry_t *e = (page_entry_t*)ent;
        assert(!PAGE_IS_DIRTY(e->page));
        pmm_releasePage(e->page);
        slab_free(page_entry_cache, e);
    }

    // Destroy
    xa_destroy(&c->xa);

    // Now update the cache list
    mutex_acquire(&cache_list_lock);
    DLIST_REMOVE(&cache_list, page_cache_t, c, node);
    mutex_release(&cache_list_lock);

    slab_free(page_cache_cache, c);
}

/**
 * @brief Mark a page as dirty (mappings with mmap will always mark as dirty)
 * @param page The page to mark as dirty
 */
void cache_markDirty(pmm_page_t *page) {
    if (page->flags & PAGE_FLAG_PERMANENT) return; // permanent pages like those in tmpfs do not need syncing

    page_cache_t *c = page->inode->cache;
    CACHE_DEFINE_PAGE_RANGE(range, 1);
    range->npages = 1;
    range->offset = page->offset;
    range->pages[0] = pmm_address(page);;

    cache_markRangeDirty(c, range);
}

/**
 * @brief Insert a dirty page into the skip list
 * Expects cache write lock held
 */
static void cache_insertDirty(page_cache_t *c, page_entry_t *e) {
    // standard skip list insertion
    page_entry_t *update[CACHE_MAX_LEVEL] = { NULL };

    // a little annoying but
    page_entry_t *cur = NULL;
    for (int i = CACHE_MAX_LEVEL-1; i >= 0; i--) {
        page_entry_t *next = cur ? cur->dirty[i] : c->dirty.dirty[i];
        while (next && next->page->offset < e->page->offset) {
            cur = next;
            next = cur->dirty[i];
        }

        if (next && e->page->offset == next->page->offset) {
            LOG(ERR, "Reinserting page into dirty list? This is a bug.\n");
            return;
        }

        update[i] = cur;
    }

    // generate a random level
    int lvl = 1;
    while (lvl < CACHE_MAX_LEVEL && (rand() < RAND_MAX / 2)) {
        lvl += 1;
    }

    e->dirty_height = lvl;

    for (int i = 0; i < lvl; i++) {
        page_entry_t **l = update[i] ? &update[i]->dirty[i] : &c->dirty.dirty[i];
        e->dirty[i] = *l;
        *l = e;
    }

    // NULL the rest of the links out
    for (unsigned i = lvl; i < CACHE_MAX_LEVEL; i++) e->dirty[i] = NULL;
    e->in_dirty_list = true;
}

/**
 * @brief Mark a range of pages as dirty
 * @param cache The cache to dirty these pages in
 * @param range The range to mark as dirty
 */
void cache_markRangeDirty(page_cache_t *cache, page_range_t *range) {
    // The pages are fully sequential
    bool have_dirty = false;
    int ndirty = 0;
    for (unsigned i = 0; i < range->npages; i++) {
        assert(range->pages[i]);
        pmm_page_t *pg = pmm_page(range->pages[i]);
        
        if (pg->flags & PAGE_FLAG_PERMANENT) {
            // permanent pages are NEVER synced back to disk
            // TODO think of a case where a range could be half permanent, its just tmpfs right now and all tmpfs pages are permanent
            return;
        }

        if (!PAGE_MARK_DIRTY(pg)) {
            if (!PAGE_IS_WRITEBACK(pg)) {
                pmm_retainPage(pg);
            } else {
                // the page is actively being written back. do not retain it
            }

            ndirty++;
            have_dirty = true;
        }
    }

    if (!have_dirty) return;

    // account these pages
    __atomic_add_fetch(&pages_dirty, ndirty, __ATOMIC_SEQ_CST);

    CACHE_START_WRITE(cache);

    unsigned rindex = 0;
    while (rindex < range->npages) {
        pmm_page_t *rpage = pmm_page(range->pages[rindex]);
        page_entry_t *ent = rpage->ent;

        if (ent->in_dirty_list) {
            // from prior write
            rindex++;
            continue;
        }

        if (PAGE_IS_WRITEBACK(ent->page)) {
            // page is being written back, it will be re-inserted.
            rindex++;
            continue;
        }

        cache_insertDirty(cache, ent);

        rindex++;
    }

    CACHE_FINISH_WRITE(cache);

    if (__atomic_exchange_n(&cache->dirty.is_dirty, true, __ATOMIC_SEQ_CST) == false) {
        // This cache is now dirty, but it might also have already been processed and is still in the list
        mutex_acquire(&dirty_lock);

        // Hack so that syncInode can avoid removing it from the dirty list.. cache_sync() wont screw this up
        if (cache->dirty.on_dirty_list == false) {
            cache->dirty.on_dirty_list = true;
            DLIST_INSERT_TAIL(&dirty_list, cache, dirty.node);
        }

        mutex_release(&dirty_lock);
    }
}

/**
 * @brief Evict while cache is locked
 */
static void cache_evictLocked(pmm_page_t *page) {
    if (page->flags & PAGE_FLAG_PERMANENT) return;
    if (PAGE_IS_DIRTY(page) || PAGE_IS_WRITEBACK(page)) {
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
    CACHE_START_WRITE(c);
    cache_evictLocked(page);
    CACHE_FINISH_WRITE(c);
}

/**
 * @brief Writeback finished
 */
static void cache_writebackFinish(page_cache_t *c, page_range_t *r) {
    CACHE_START_WRITE(c);

    for (unsigned i = 0; i < r->npages; i++) {
        pmm_page_t *pg = pmm_page(r->pages[i]);
        page_entry_t *ent = pg->ent;
        
        // clear the writeback flag
        PAGE_CLEAR_WRITEBACK(pg);

        if (PAGE_IS_DIRTY(pg)) {
            // the dirty flag was cleared on syncInodeInner - this means that
            // a page was re-dirtied. reinsert the page.
            assert(ent->in_dirty_list == false);
            cache_insertDirty(c, ent); // dont release the reference on the page, it will remain pinned
        } else {
            pmm_releasePage(pg);
        }
    }

    CACHE_FINISH_WRITE(c);
}

/**
 * @brief Inner sync assuming inode was removed from cache list
 * Dirty points to the first level of the skip list
 */
static int cache_syncInodeInner(vfs_inode_t *inode, page_entry_t *dirty) {
    if (!dirty) return 0;
    page_cache_t *c = inode->cache;
    page_entry_t *curr = dirty;
    page_range_t *range = kmalloc(sizeof(page_range_t) + sizeof(uintptr_t)*CACHE_SYNC_MAX_PAGES);
    page_entry_t *cur = dirty;
    while (cur) {
        range->npages = 0;
        range->offset = 0;
        loff_t expected_offset = 0;
        while (cur && range->npages < CACHE_SYNC_MAX_PAGES) {
            pmm_page_t *pg = cur->page;
            page_entry_t *dnext = cur->dirty[0];

            // if this page was truncated try again
            if (PAGE_IS_TRUNCATED(pg)) {
                __atomic_sub_fetch(&pages_dirty, 1, __ATOMIC_RELAXED);
                pmm_releasePage(pg);
                slab_free(page_entry_cache, cur);
                cur = dnext;
                break;
            }

            // out of pages remaining
            if (range->npages > 0 && pg->offset != expected_offset) {
                break;
            }

            uint32_t saved_fl = __atomic_load_n(&pg->flags, __ATOMIC_SEQ_CST);
            uint32_t new_fl;
            bool page_claimed = true;
            
            do {
                if ((saved_fl & PAGE_FLAG_WRITEBACK) || !(saved_fl & PAGE_FLAG_DIRTY)) {
                    // TODO Maybe just resync the page if dirty
                    page_claimed = false;
                    LOG(DEBUG, "A page is already being synced\n");
                    break;
                }
                
                new_fl = (saved_fl & ~(PAGE_FLAG_DIRTY)) | PAGE_FLAG_WRITEBACK;
            } while (!__atomic_compare_exchange_n(&pg->flags, &saved_fl, new_fl, true, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST));

            if (page_claimed == false) {
                // some other thread is syncing this page
                pmm_releasePage(pg);
                cur = dnext;
                break;
            }

            if (range->npages == 0) {
                // first page in the list
                range->offset = pg->offset;
                expected_offset = pg->offset;
            }

            // add it to the range list
            range->pages[range->npages] = pmm_address(pg);
            range->npages += 1;
            expected_offset += PAGE_SIZE;
            cur->in_dirty_list = false;
            cur->dirty_height = 0;
            cur = dnext;
        }

        if (range->npages > 0) {
            int r = -ENXIO;
            if (inode->c_ops->write_range) {
                r = inode->c_ops->write_range(inode, range);
            }

            assert(r >= 0 && "TODO: error handling on cache_syncInodeInner");
            __atomic_sub_fetch(&pages_dirty, range->npages, __ATOMIC_RELAXED);

            cache_writebackFinish(c, range);
        }
    }



    kfree(range);
    return 0;
}

/**
 * @brief Sync all dirty pages in an inode
 * @param inode The inode to sync
 */
int cache_syncInode(vfs_inode_t *inode) {
    page_cache_t *c = inode->cache;

    // sync_lock must be held through the entire process to prevent racing
    mutex_acquire(&c->dirty.sync_lock);

    // remove the page cache from the dirty list
    mutex_acquire(&dirty_lock);
    if (c->dirty.on_dirty_list) {
        DLIST_REMOVE(&dirty_list, page_cache_t, c, dirty.node);
        c->dirty.on_dirty_list = false;
    }
    mutex_release(&dirty_lock);

    // take from the skip list
    CACHE_START_WRITE(c);
    page_entry_t *popped = c->dirty.dirty[0];
    memset(c->dirty.dirty, 0, sizeof(c->dirty.dirty));
    __atomic_store_n(&c->dirty.is_dirty, false, __ATOMIC_SEQ_CST);
    CACHE_FINISH_WRITE(c);

    // sync the inode
    int r = cache_syncInodeInner(inode, popped);

    mutex_release(&c->dirty.sync_lock);

    vfs_syncFilesystem(inode->mount);
    
    return r;
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
    mutex_acquire(&c->dirty.sync_lock); // no syncing can take place during this
    CACHE_START_WRITE(c);

    loff_t start_loss = PAGE_ALIGN_UP(new_size);
    unsigned long idx = start_loss / PAGE_SIZE;
    void *ent = xa_find(&c->xa, &idx, ULONG_MAX);
    while (ent) {
        page_entry_t *e = ent;
        pmm_page_t *page = e->page;

        if (PAGE_IS_DIRTY(page)) {
            PAGE_MARK_TRUNC(page);
            cache_remove(c, page);
            pmm_releasePage(page);
        } else {
            cache_remove(c, page);
            pmm_releasePage(page);
            slab_free(page_entry_cache, e);
        }

        ent = xa_next(&c->xa, &idx);
    }

    CACHE_FINISH_WRITE(c);
    mutex_release(&c->dirty.sync_lock);
}

/**
 * @brief Sync cache now
 */
void cache_sync() {
    while (true) {
        // pop a cache from the list
        mutex_acquire(&dirty_lock);
        page_cache_t *cache = DLIST_FIRST(&dirty_list);
        if (!cache) {
            mutex_release(&dirty_lock);
            break;
        }

        DLIST_REMOVE(&dirty_list, page_cache_t, cache, dirty.node);
        cache->dirty.on_dirty_list = false;
        mutex_release(&dirty_lock);

        mutex_acquire(&cache->dirty.sync_lock);
        if (!__atomic_load_n(&cache->dirty.is_dirty, __ATOMIC_SEQ_CST)) {
            goto _next;
        }

        CACHE_START_WRITE(cache);
        page_entry_t *epop = cache->dirty.dirty[0];
        memset(cache->dirty.dirty, 0, sizeof(cache->dirty.dirty));
        __atomic_store_n(&cache->dirty.is_dirty, false, __ATOMIC_SEQ_CST);
        CACHE_FINISH_WRITE(cache);

        if (!epop) {
            goto _next;
        }

        if (cache_syncInodeInner(epop->page->inode, epop) != 0) {
            LOG(WARN, "syncInodeInner failed\n");
        }

    _next:
        mutex_release(&cache->dirty.sync_lock);
    }

    // sync every filesystem
    vfs_syncFilesystems();
}

/**
 * @brief Get active pages
 */
int cache_active() {
    return __atomic_load_n(&pages_active, __ATOMIC_RELAXED);
}

/**
 * @brief Get dirty pages
 */
int cache_dirty() {
    return __atomic_load_n(&pages_dirty, __ATOMIC_RELAXED);
}

/**
 * @brief Cache syncer thread
 */
void cache_syncer(void *arg) {
    for (;;) {
        event_listener_t l;
        EVENT_INIT_LISTENER(&l);
        EVENT_ATTACH(&l, &sync_event);

        cache_sync();

        EVENT_WAIT(&l, -1);
        EVENT_DETACH(&l);
        EVENT_DESTROY_LISTENER(&l);
    }
}

/**
 * @brief Initialize cache pruner
 */
int cache_prunerInit() {
    process_t *cache_syncer_proc = process_createKernel("cache syncer", PROCESS_KERNEL, cache_syncer, NULL);
    sched_insert(cache_syncer_proc->main_thread);
    return 0;
}

SCHED_INIT_ROUTINE(cache, INIT_FLAG_DEFAULT, cache_prunerInit);
