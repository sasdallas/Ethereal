/**
 * @file hexahedron/include/kernel/mm/cache.h
 * @brief Page cache for files
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef KERNEL_MM_CACHE_H
#define KERNEL_MM_CACHE_H

/**** INCLUDES ****/
#include <kernel/mm/pmm.h>
#include <kernel/event.h>
#include <kernel/lock/rwsem.h>
#include <kernel/misc/xarray.h>

/**** DEFINITIONS ****/

#define PAGE_CACHE_MAP_ENTS             32  // Default map entries per hashmap

#define CACHE_FLAG_ALIGNEDWRITE         0x1 // Tells the page cache you are trying to a write op with start and end being page aligned.
                                            // The page cache automatically optimizes this by avoiding reading back

/**** TYPES ****/

typedef struct page_range {
    loff_t offset;
    size_t npages;
    uintptr_t pages[];
} page_range_t;

typedef struct page_entry {
    struct page_entry *next;
    struct page_entry *dirty_next;
    volatile char period;
    bool in_dirty_list; // stupid dumb no good hack
    bool being_evicted; // used by syncer thread
    pmm_page_t *page;
} page_entry_t;

typedef struct page_cache {
    struct page_cache *next;
    struct page_cache *prev;
    rwsem_t sem;
    xarray_t xa;

    struct {
        struct page_cache *next;
        page_entry_t *head;
        bool is_dirty;
    } dirty;

    spinlock_t ready_event_lock;
    event_t ready_event;
} page_cache_t;


/**** MACROS ****/

#define CACHE_START_READ(c) rwsem_startRead(&(c)->sem)
#define CACHE_START_WRITE(c) rwsem_startWrite(&(c)->sem)
#define CACHE_FINISH_READ(c) rwsem_finishRead(&(c)->sem)
#define CACHE_FINISH_WRITE(c) rwsem_finishWrite(&(c)->sem)

#define CACHE_DEFINE_PAGE_RANGE(name,_npages) char name##_storage[sizeof(page_range_t) + (sizeof(uintptr_t)*(_npages))];\
                                             page_range_t *name = (page_range_t*)name##_storage;

/**** FUNCTIONS ****/

/**
 * @brief Initialize page cache
 */
void cache_init();

/**
 * @brief Initialize cache for inode
 */
page_cache_t* cache_create() ;

/**
 * @brief Get a page from the page cache
 * @param inode The VFS inode to retrieve
 * @param offset The offset in the file to retrieve a page (page-aligned)
 * @param output The output page address
 * @returns 0 on success
 */
int cache_getPage(struct vfs_inode *inode, loff_t offset, pmm_page_t **output);

/**
 * @brief Get a page range
 * @param inode The inode to get the page range for
 * @param offset The offset in the inode to retrieve a range
 * @param npages The number of pages to receive
 * @param range The range to output to
 * @param flags Flags
 * @returns 0 on success
 */
int cache_getRange(struct vfs_inode *inode, loff_t offset, size_t npages, page_range_t *range, int flags);

/**
 * @brief Release a range of pages
 * @param range The range to release
 */
void cache_releaseRange(page_range_t *range);

/**
 * @brief Mark a page as dirty (mappings with mmap will always mark as dirty)
 * @param page The page to mark as dirty
 */
void cache_markDirty(pmm_page_t *page);

/**
 * @brief Mark a range of pages as dirty
 * @param cache The cache to dirty these pages in
 * @param range The range to mark as dirty
 */
void cache_markRangeDirty(page_cache_t *cache, page_range_t *range);

/**
 * @brief Evict a page from the cache
 * @param page The page to evict from the cache
 */
void cache_evict(pmm_page_t *page);

/**
 * @brief Truncate a cache for an inode
 * @param inode The inode cache to truncate
 * @param new_size The new size of the cache
 */
void cache_truncate(struct vfs_inode *inode, loff_t new_size);

/**
 * @brief Destroy and free an inode cache
 */
void cache_destroy(struct vfs_inode *inode);

/**
 * @brief Get active pages
 */
int cache_active();

/**
 * @brief Get dirty pages
 */
int cache_dirty();

/**
 * @brief Sync cache now
 */
void cache_sync();

/**
 * @brief Sync all dirty pages in an inode
 * @param inode The inode to sync
 */
int cache_syncInode(struct vfs_inode *inode);

#endif