/**
 * @file drivers/fs/ext2/bcache.c
 * @brief ext2 block cache
 * 
 * I architected this to use a hashmap and LRU list for reclamation
 * On mount-time the system allocates @c EXT2_BCACHE_POOL_SIZE buffer objects and gives them
 * each a virtual page in memory.
 * 
 * This is more of a metadata cache and should never be called for anything except metadata
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#include "ext2.h"
#include <kernel/mm/vmm.h>
#include <kernel/debug.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "DRIVER:EXT2:BCACHE", __VA_ARGS__)

static ext2_buffer_t *ext2_bcacheFindHash(ext2_bcache_t *bc, uint64_t page) {
    ext2_buffer_t *b = bc->hash[page % EXT2_BCACHE_MAX_HASH];

    while (b) {
        if (b->device_page == page) {
            return b;
        }

        b = b->next;
    }

    return NULL;
}

static void ext2_bcacheInsertHash(ext2_bcache_t *bc, ext2_buffer_t *buffer) {
    unsigned pg = buffer->device_page % EXT2_BCACHE_MAX_HASH;
    ext2_buffer_t *existing = bc->hash[pg];
    buffer->next = existing;
    buffer->prev = NULL;
    if (existing) existing->prev = buffer;
    bc->hash[pg] = buffer;
}

static void ext2_bcacheRemoveHash(ext2_bcache_t *bc, ext2_buffer_t *buffer) {
    unsigned pg = buffer->device_page % EXT2_BCACHE_MAX_HASH;

    if (buffer->prev == NULL) {
        // this is the first buffer in the list
        if (buffer->next) buffer->next->prev = NULL;
        bc->hash[pg] = buffer->next;
    } else {
        if (buffer->next) buffer->next->prev = buffer->prev;
        buffer->prev->next = buffer->next;
    }
}

static void ext2_bcacheTouchLru(ext2_bcache_t *bc, ext2_buffer_t *buffer) {
    DLIST_REMOVE(&bc->lru, ext2_buffer_t, buffer, lru);
    DLIST_INSERT_HEAD(&bc->lru, buffer, lru);
}

static void ext2_bcacheRemoveLru(ext2_bcache_t *bc, ext2_buffer_t *buffer) {
    DLIST_REMOVE(&bc->lru, ext2_buffer_t, buffer, lru);
}

static inline void ext2_bcacheInsertLru(ext2_bcache_t *bc, ext2_buffer_t *buffer) {
    DLIST_INSERT_HEAD(&bc->lru, buffer, lru);
}

// finds the first unreferenced entry
static ext2_buffer_t *ext2_bcacheFindLru(ext2_bcache_t *bc) {
    DLIST_FOREACH_REVERSE(ext2_buffer_t, buf, &bc->lru, lru) {
        if (buf->refs == 0) {
            return buf;
        }
    }

    assert(0 && "no unpinned buffers available in LRU cache?");
}

void ext2_bcacheInit(ext2_t *fs) {
    MUTEX_INIT(&fs->bcache.lock);
    DLIST_INIT(&fs->bcache.lru);
    
    // Prepare the pools with each getting a chunk of virtual memory
    // TODO memory usage

    for (int i = 0; i < EXT2_BCACHE_POOL_SIZE; i++) {
        ext2_buffer_t *buf = &fs->bcache.pool[i];
        void *a = vmm_map(NULL, PAGE_SIZE, VM_FLAG_ALLOC, MMU_FLAG_PRESENT | MMU_FLAG_WRITE);
        assert(a != NULL);

        buf->phys_backing = arch_mmu_physical(NULL, (uintptr_t)a);
        assert(buf->phys_backing != 0x0);

        MUTEX_INIT(&buf->lock);
        refcount_init(&buf->refs, 0);
        buf->valid = false;
        buf->dirty = false;
        buf->device_page = EXT2_BCACHE_NONE;
        buf->data = a;

        ext2_bcacheInsertLru(&fs->bcache, buf);
    }


    DLIST_FOREACH_REVERSE(ext2_buffer_t, buf, &fs->bcache.lru, lru) {
        LOG(DEBUG, "Buffer %p\n", buf);
    }
}

void ext2_bcacheDestroy(ext2_t *fs) {
    for (int i = 0; i < EXT2_BCACHE_POOL_SIZE; i++) {
        ext2_buffer_t *buf = &fs->bcache.pool[i];
        vmm_unmap(buf->data, PAGE_SIZE);
    }
}

static ext2_buffer_t *ext2_bcacheGetInternal(ext2_t *fs, uint64_t page, bool initialize) {

    ext2_bcache_t *bc = &fs->bcache;
    mutex_acquire(&bc->lock);

    // first check the hash list to see if it is valid
    ext2_buffer_t *buffer = ext2_bcacheFindHash(bc, page);
    if (buffer) {
        // Yes, pin it
        refcount_inc(&buffer->refs);
        ext2_bcacheTouchLru(bc, buffer);
        mutex_release(&bc->lock);
        mutex_acquire(&buffer->lock);
    } else {
        // No, we have to load it. Find the first unpinned
        buffer = ext2_bcacheFindLru(bc);

        uint64_t flush_page = EXT2_BCACHE_NONE;
        if (buffer->device_page != EXT2_BCACHE_NONE) {
            // this was previously loaded
            if (buffer->dirty) flush_page = buffer->device_page;
            ext2_bcacheRemoveHash(bc, buffer);
        }

        refcount_inc(&buffer->refs);
        buffer->device_page = page;
        buffer->valid = false; // needs a re-read

        // insert it here, even if something else catches it, it will just be read anyways 
        // that is, unless we have to do a writeback first, then we have to do worst-case locking
        if (flush_page == EXT2_BCACHE_NONE) {
            ext2_bcacheInsertHash(bc, buffer);
        }

        ext2_bcacheTouchLru(bc, buffer);
        mutex_release(&bc->lock);

        if (flush_page != EXT2_BCACHE_NONE) {
            // the buffer right now isn't owned by anyone except for us, so locking it isn't a necessity
            int r = ext2_bio(fs, (loff_t)flush_page * PAGE_SIZE, &buffer->phys_backing, 1, true);
            if (r != 0) {
                LOG(ERR, "Failed to writeback metadata block at page %llu (error %d)\n", flush_page, r);
                LOG(ERR, "Filesystem corruption is likely!\n");
                ext2_markError(fs);
            }

            buffer->dirty = false;

            // now we can insert it into the hash list
            mutex_acquire(&bc->lock);
            ext2_bcacheInsertHash(bc, buffer);
            mutex_release(&bc->lock);
        }

        mutex_acquire(&buffer->lock);
    }

    // buffer lock is held here
    if (buffer->valid == false) {
        if (initialize) {
            // page doesn't matter and will be overwritten anyways
            memset(buffer->data, 0, PAGE_SIZE);
        } else {
            // page does kinda matter
            int r = ext2_bio(fs, (loff_t)page * PAGE_SIZE, &buffer->phys_backing, 1, false);
            if (r != 0) {
                LOG(ERR, "Failed to read buffer at physical page %llu (error code %d)\n", page, r);
                ext2_bcacheFinish(fs, buffer);
                return NULL;
            }
        }

        buffer->valid = true;
        buffer->dirty = false;
    }

    return buffer;
}

// returns with buffer lock held!
ext2_buffer_t *ext2_bcacheGet(ext2_t *fs, uint64_t page) {
    return ext2_bcacheGetInternal(fs, page, false);
}

// returns with buffer lock held!
ext2_buffer_t *ext2_bcacheGetZero(ext2_t *fs, uint64_t page) {
    return ext2_bcacheGetInternal(fs, page, true);
}

void ext2_bcacheFinish(ext2_t *fs, ext2_buffer_t *buffer) {
    // the lock must be dropped first, since the atomic refcount drop might cause it to get freed
    // we can gurantee however that the buffer remains valid since it has at least 1 ref
    mutex_release(&buffer->lock);
    refcount_dec(&buffer->refs);
}

int ext2_bcacheSync(ext2_t *fs) {
    int ret = 0;
    ext2_bcache_t *bc = &fs->bcache;
    for (int i = 0; i < EXT2_BCACHE_POOL_SIZE; i++) {
        ext2_buffer_t *b = &bc->pool[i];
        if (!b->data) continue;
        
        mutex_acquire(&bc->lock);
        if (b->device_page == EXT2_BCACHE_NONE || !b->dirty) {
            mutex_release(&bc->lock);
            continue;
        }

        refcount_inc(&b->refs);
        mutex_release(&bc->lock);

        mutex_acquire(&b->lock);
        int r = ext2_bio(fs, b->device_page * PAGE_SIZE, &b->phys_backing, 1, true);
        if (!r) b->dirty = false;
        mutex_release(&b->lock);

        if (r != 0) {
            LOG(ERR, "An error occurred syncing metadata buffer at device_page 0x%llx (%d)\n", b->device_page, r);
            ext2_markError(fs);
            ret = r;
        }

        mutex_acquire(&bc->lock);
        refcount_dec(&b->refs);
        mutex_release(&bc->lock);
    }

    return ret;
}
