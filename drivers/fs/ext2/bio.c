/**
 * @file drivers/fs/ext2/bio.c
 * @brief Block I/O
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#include "ext2.h"
#include <kernel/fs/vfs_new.h>
#include <kernel/mm/cache.h>
#include <kernel/misc/util.h>
#include <kernel/debug.h>
#include <assert.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "DRIVER:EXT2:BIO", __VA_ARGS__)

int ext2_bio(ext2_t *fs, loff_t off, uintptr_t *pages, size_t npages, bool write) {
    if (!npages) return 0;
    assert((off & (PAGE_SIZE - 1)) == 0);

    vfs_inode_t *dev = fs->backing->inode;
    if (!dev->c_ops || (write ? !dev->c_ops->write_range : !dev->c_ops->read_range)) {
        assert(0 && "support for devices that do not implement write_range and read_range is not implemented");
    }

    char storage[sizeof(page_range_t) + (sizeof(uintptr_t) * EXT2_MAX_BIO_PAGES)];
    page_range_t *range = (page_range_t*)storage;

    while (npages) {
        size_t n = min(npages, (size_t)EXT2_MAX_BIO_PAGES);

        range->offset = off;
        range->npages = n;
        memcpy(range->pages, pages, n * sizeof(uintptr_t));

        int r = write ? dev->c_ops->write_range(dev, range) : dev->c_ops->read_range(dev, range);
        if (r < 0) {
            LOG(ERR, "Device %s failed at offset %lld (%d pages, error %d)\n", write ? "WRITE" : "READ", off, (int)n, r);
            return r;
        }

        off += n * PAGE_SIZE;
        pages += n;
        npages -= n;
    }

    return 0;
}

char *ext2_blockGet(ext2_t *fs, uint32_t blk, ext2_buffer_t **out) {
    loff_t off = EXT2_BLOCK_TO_DISK(fs, blk);

    ext2_buffer_t *b = ext2_bcacheGet(fs, off / PAGE_SIZE);
    if (!b) return NULL;

    *out = b;
    return b->data + (off & (PAGE_SIZE - 1));
}

// !!! DOES NOT ZERO THE BLOCK, DO THAT YOURSELF
char *ext2_blockGetZero(ext2_t *fs, uint32_t blk, ext2_buffer_t **out) {
    loff_t off = EXT2_BLOCK_TO_DISK(fs, blk);

    // we can't get a zeroed block if there's more than one page
    ext2_buffer_t *b = (fs->blocks_per_page == 1) ? ext2_bcacheGetZero(fs, off / PAGE_SIZE) : ext2_bcacheGet(fs, off / PAGE_SIZE);
    if (!b) return NULL;

    *out = b;
    return b->data + (off & (PAGE_SIZE - 1));
}

int ext2_blockRead(ext2_t *fs, uint32_t blk, void *dst) {
    ext2_buffer_t *b;
    char *p = ext2_blockGet(fs, blk, &b);
    if (!p) return -EIO;

    memcpy(dst, p, fs->blksize);
    ext2_bcacheFinish(fs, b);
    return 0;
}

int ext2_blockWrite(ext2_t *fs, uint32_t blk, void *src) {
    ext2_buffer_t *b;
    char *p = ext2_blockGetZero(fs, blk, &b);
    if (!p) return -EIO;

    memcpy(p, src, fs->blksize);
    b->dirty = true;
    ext2_bcacheFinish(fs, b);
    return 0;
}

int ext2_blockZero(ext2_t *fs, uint32_t blk) {
    ext2_buffer_t *b;
    char *p = ext2_blockGetZero(fs, blk, &b);
    if (!p) return -EIO;

    memset(p, 0, fs->blksize);
    b->dirty = true;
    ext2_bcacheFinish(fs, b);
    return 0;
}
