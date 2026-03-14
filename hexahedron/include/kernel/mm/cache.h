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

/**** DEFINITIONS ****/

#define PAGE_CACHE_MAP_ENTS             10  // Default map entries per hashmap

/**** TYPES ****/

typedef struct page_entry {
    struct page_entry *next;
    volatile char period;
    pmm_page_t *page;
} page_entry_t;

typedef struct page_cache {
    mutex_t lck;
    page_entry_t *page_map[PAGE_CACHE_MAP_ENTS];

    spinlock_t ready_event_lock;
    event_t ready_event;
} page_cache_t;

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
 * @param node The VFS node to retrieve
 * @param offset The offset in the file to retrieve a page (page-aligned)
 * @param output The output page address
 * @returns 0 on success
 */
int cache_getPage(struct vfs_file *node, loff_t offset, pmm_page_t **output);

/**
 * @brief Mark a page as dirty (mappings with mmap will always mark as dirty)
 * @param page The page to mark as dirty
 */
void cache_markDirty(pmm_page_t *page);

/**
 * @brief Evict a page from the cache
 * @param page The page to evict from the cache
 */
void cache_evict(pmm_page_t *page);

#endif