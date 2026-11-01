/**
 * @file drivers/fs/ext2/map.c
 * @brief EXT2 block mapper
 * 
 * Maps contiguous physical blocks for contiguous logical blocks
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
#include <kernel/misc/util.h>
#include <string.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "DRIVER:EXT2:MAP", __VA_ARGS__)

/* Location describing one array of pointers */
typedef struct ext2_location {
    uint32_t *ptrs;
    ext2_buffer_t *buf;
    uint32_t idx;
    size_t navail; // size of pointers array and amount remaining
} ext2_location_t;

// allocated new blocks
static inline void ext2_nodeAllocedBlocks(ext2_node_t *n, uint32_t start, size_t count) {
    n->last_alloc = (start + count - 1);
    n->ino.disk_sectors += ((uint32_t)(count * (n->fs->blksize / 512))); 
    n->dirty = true;
}

// freed old blocks
static inline void ext2_nodeFreedBlocks(ext2_node_t *n, size_t count) {
    uint32_t sectors = (uint32_t)(count * (n->fs->blksize / 512));
    n->ino.disk_sectors = (n->ino.disk_sectors > sectors) ? (n->ino.disk_sectors - sectors) : 0;
    n->dirty = true;
}

// get hint, using group if not available
static inline uint32_t ext2_hint(ext2_node_t *n) {
    if (n->last_alloc != 0) {
        return n->last_alloc + 1;
    }

    uint32_t g = EXT2_INO_GROUP(n->fs, n->num);
    if (g >= n->fs->ngroups) assert(0);;
    return (n->fs->sb.start_block_number + (g * n->fs->sb.bg_block_count));
}

// gets the root of an inode indirect pointer and allocs if missing (root SIBP, root DIBP, root TIBP)
// slot points to the inode's field for the thing
static int ext2_root(ext2_node_t *n, uint32_t *slot, bool alloc, uint32_t *out) {
    if (*slot || !alloc) {
        *out = *slot;
        return 0;
    }

    uint32_t start;
    size_t got = 0;
    int r = ext2_alloc(n->fs, ext2_hint(n), 1, &start, &got);
    if (r != 0) return r;

    if (got == 0) {
        return -ENOSPC;
    }

    r = ext2_blockZero(n->fs, start);
    if (r != 0) {
        // TODO: call ext2_freeBlocks
        LOG(ERR, "ext2_blockZero failed with error code %d (leaving block ALLOCATED!)\n", r);
        ext2_markError(n->fs);
        return r;
    }

    *slot = start;
    *out = start;
    ext2_nodeAllocedBlocks(n, start, got);

    return 0;
}

// get a slot within an indirect pointer, allocing if missing
static int ext2_slot(ext2_node_t *n, uint32_t blk, uint32_t idx, bool alloc, uint32_t *out) {
    ext2_t *fs = n->fs;

    // get the pointers
    ext2_buffer_t *buf;
    uint32_t *ptrs = (uint32_t*)ext2_blockGet(fs, blk, &buf);
    if (!ptrs) return -EIO;

    uint32_t child = ptrs[idx];
    ext2_bcacheFinish(fs, buf);

    if (child || !alloc) {
        *out = child;
        return 0;
    }

    // there was none, we have to allocate it
    uint32_t start;
    size_t got = 0;
    int r = ext2_alloc(fs, ext2_hint(n), 1, &start, &got);
    if (r != 0) return r;

    if (got == 0) {
        ext2_markError(fs);
        return -ENOSPC;
    }

    // zero the new block
    r = ext2_blockZero(fs, start);
    if (r != 0) {
        // TODO: call ext2_freeBlocks
        LOG(ERR, "ext2_blockZero failed with error code %d (leaving block ALLOCATED!)\n", r);
        ext2_markError(n->fs);
        return r;
    }

    ptrs = (uint32_t*)ext2_blockGet(fs, blk, &buf);
    if (!ptrs) {
        LOG(ERR, "ext2_blockGet failed with error code %d (leaving block ALLOCATED!)\n", r);
        ext2_markError(n->fs);
        return r;
    }

    ptrs[idx] = start;

    // mark the buffer as dirty
    buf->dirty = true;
    ext2_bcacheFinish(fs, buf);

    ext2_nodeAllocedBlocks(n, start, got);
    *out = start;
    return 0;
}

static int ext2_locate(ext2_node_t *node, uint32_t lblk, bool alloc, ext2_location_t *loc) {
    ext2_t *fs = node->fs;

    loc->ptrs = NULL;
    loc->buf = NULL;
    loc->idx = 0;
    loc->navail = 0;

    // inode has 12 DBPs that we can piggyback off of
    if (lblk < EXT2_DIRECT_BLOCK_POINTERS) {
        loc->ptrs = node->ino.dbp;
        loc->idx = lblk;
        loc->navail = EXT2_DIRECT_BLOCK_POINTERS - lblk;
        return 0;
    }

    // let's go
    uint64_t i = lblk - EXT2_DIRECT_BLOCK_POINTERS;
    uint64_t p = fs->ptrs_per_block;

    // singly indirect
    if (i < p) {
        uint32_t blk;
        int r = ext2_root(node, &node->ino.sibp, alloc, &blk);
        if (r != 0) return r;

        loc->navail = (p - i);
        if (blk == 0) return 0;

        loc->ptrs = (uint32_t*)ext2_blockGet(fs, blk, &loc->buf);
        if (!loc->ptrs) return -EIO;

        loc->idx = (uint32_t)i;
        return 0;
    }

    // doubly indirect
    i -= p;
    if (i < p*p) {
        uint32_t l1;
        int r = ext2_root(node, &node->ino.dibp, alloc, &l1);
        if (r != 0) return r;

        if (!l1) {
            loc->navail = p * p - i;
            return 0;
        }

        uint32_t leaf;
        r = ext2_slot(node, l1, (uint32_t)(i / p), alloc, &leaf);
        if (r != 0) return r;

        loc->navail = (p - (i % p));
        if (!leaf) return 0;

        loc->ptrs = (uint32_t*)ext2_blockGet(fs, leaf, &loc->buf);
        if (!loc->ptrs) return -EIO;

        loc->idx = (uint32_t)(i % p);
        return 0;
    }

    // triply indirect
    i -= p*p;
    if (i < p*p*p) {
        uint32_t l1;
        int r = ext2_root(node, &node->ino.tibp, alloc, &l1);
        if (r != 0) return r;

        if (!l1) {
            loc->navail = p * p * p - i;
            return 0;
        }

        uint32_t l2;
        r = ext2_slot(node, l1, (uint32_t)(i / (p * p)), alloc, &l2);
        if (r != 0) return r;

        if (!l2) {
            loc->navail = (p * p - (i % (p * p)));
            return 0;
        }

        uint32_t leaf;
        r = ext2_slot(node, l2, (uint32_t)((i % (p * p)) / p), alloc, &leaf);
        if (r != 0) return r;

        loc->navail = (p - (i % p));
        if (!leaf) return 0;

        loc->ptrs = (uint32_t*)ext2_blockGet(fs, leaf, &loc->buf);
        if (!loc->ptrs) return -EIO;

        loc->idx = (uint32_t)(i % p);
        return 0;

    }

    assert(0 && "exceeded TIBP..?"); 
}

static void ext2_finishLoc(ext2_t *fs, ext2_location_t *loc) {
    if (loc->buf) {
        ext2_bcacheFinish(fs, loc->buf);
        loc->buf = NULL;
    }
}

int ext2_map(ext2_node_t *n, uint32_t lblk, uint32_t want, bool alloc, ext2_extent_t *out) {
    ext2_t *fs = n->fs;
    if (want == 0) want = 1;

    // Locate the run
    ext2_location_t loc;
    int r = ext2_locate(n, lblk, alloc, &loc);
    if (r != 0) return r;

    if (!loc.ptrs) {
        // the indirect block doesn't exist
        out->lblk = lblk;
        out->pblk = 0;
        out->len = min(want, loc.navail ? loc.navail : 1);
        return 0;
    }

    uint32_t limit = min(want, loc.navail);
    if (limit == 0) limit = 1;

    uint32_t first = loc.ptrs[loc.idx];
    uint32_t len = 1;

    if (first) {
        // Extend the run
        while (len < limit && loc.ptrs[loc.idx + len] == first + len) {
            len++;
        }

        ext2_finishLoc(fs, &loc);
        out->lblk = lblk;
        out->pblk = first;
        out->len = len;

        return 0;
    }

    // get consecutive pointers for one run
    while (len < limit && loc.ptrs[loc.idx + len] == 0) {
        len++;
    }

    if (!alloc) {
        // aint allocating anything
        ext2_finishLoc(fs, &loc);
        out->lblk = lblk;
        out->pblk = 0;
        out->len = len;
        return 0;
    }

    // We have to allocate more, try allocating at the end of whatever extent we got
    uint32_t hint;
    if (loc.idx > 0 && loc.ptrs[loc.idx - 1]) {
        hint = loc.ptrs[loc.idx - 1] + 1;
    } else {
        hint = ext2_hint(n);
    }

    // Allocate
    uint32_t start;
    size_t got;
    r = ext2_alloc(fs, hint, len, &start, &got);
    if (r != 0) {
        ext2_finishLoc(fs, &loc);
        return r;
    }

    // Move them into the pointer array
    for (size_t i = 0; i < got; i++) {
        loc.ptrs[loc.idx + i] = start + (uint32_t)i;
    }

    if (loc.buf) {
        loc.buf->dirty = true;
    } else {
        n->dirty = true;
    }

    ext2_finishLoc(fs, &loc);
    ext2_nodeAllocedBlocks(n, start, got);
    
    out->lblk = lblk;
    out->pblk = start;
    out->len = (uint32_t)got;
    return 0;
}

// helper to prune IBPs, recurses
static int ext2_prune(ext2_node_t *n, uint32_t *slot, int level, uint64_t base, uint32_t from) {
    ext2_t *fs = n->fs;
    if (!*slot) return 0;


    // read the pointer block
    uint32_t *ptrs = kmalloc(fs->blksize);
    int r = ext2_blockRead(fs, *slot, ptrs);
    if (r != 0) {
        kfree(ptrs);
        return r;
    }

    // how many pointers does this block span?
    uint64_t span = 1;
    for (int i = 1; i < level; i++) {
        span = span * fs->ptrs_per_block;
    }

    bool did_anything = false;
    uint32_t i = 0;
    while (i < fs->ptrs_per_block) {
        uint64_t cbase = base + ((uint64_t)i * span);
        if (cbase + span <= from) {
            i += 1;
            continue;
        }

        if (!ptrs[i]) {
            i += 1;
            continue;
        }

        // recurse if needed
        if (level > 1) {
            r = ext2_prune(n, &ptrs[i], level - 1, cbase, from);
            if (r != 0) {
                kfree(ptrs);
                return r;
            }

            // if it was freed, this array will have a 0 in it
            if (!ptrs[i]) {
                did_anything = true;
            }

            i++;
            continue;
        } else {
            // collect runs of data blocks
            uint32_t run_start = ptrs[i];
            uint32_t run = 1;
            ptrs[i] = 0;

            while (i + run < fs->ptrs_per_block) {
                uint64_t nbase = base + ((uint64_t)(i + run) * span);
                if (nbase < from) break;
                if (ptrs[i + run] != run_start + run) break;

                ptrs[i+run] = 0;
                run++;
            }

            r = ext2_freeBlocks(fs, run_start, run);
            if (r != 0) {
                return r;
            }


            ext2_nodeFreedBlocks(n, run);
            did_anything = true;
            i += run;
        }
    }

    bool empty = true;
    for (uint32_t i = 0; i < fs->ptrs_per_block; i++) {
        if (ptrs[i]) {
            empty = false;
            break;
        }
    }

    if (empty) {
        uint32_t blk = *slot;
        kfree(ptrs);

        r = ext2_freeBlocks(fs, blk, 1);
        if (r != 0) return r;

        ext2_nodeFreedBlocks(n, 1);
        *slot = 0;
        n->dirty = true;
        return 0;
    }

    if (did_anything) r = ext2_blockWrite(fs, *slot, ptrs);
    kfree(ptrs);
    return r;
}

// truncate
int ext2_truncateInternal(ext2_node_t *n, uint64_t new_size) {
    ext2_t *fs = n->fs;

    uint64_t old_size = ext2_size(fs, &n->ino);
    if (new_size >= old_size) {
        // growing does not need immediate alloc
        goto _finish;
    }

    // otherwise, we have to prune blocks
    uint32_t new_start = EXT2_TO_BLOCKS(fs, new_size);
    
    // prune DBPs first
    if (new_start < EXT2_DIRECT_BLOCK_POINTERS) {
        uint32_t i = new_start;

        while (i < EXT2_DIRECT_BLOCK_POINTERS) {
            if (!n->ino.dbp[i]) {
                i++; continue;
            }

            // collect
            uint32_t start = n->ino.dbp[i];
            size_t count = 0;
            do {
                n->ino.dbp[i + count] = 0;
                count++;
            } while (i + count < EXT2_DIRECT_BLOCK_POINTERS && n->ino.dbp[i+count] == start+count);

            int r = ext2_freeBlocks(fs, start, count);
            if (r != 0) {
                LOG(WARN, "ext2_freeBlocks failed with error code %d\n", r);
                ext2_markError(fs);
                return r;
            }

            ext2_nodeFreedBlocks(n, count);
            i += count;
        }
    }

    // then move on to SIBP/DIBP/TIBP
    uint32_t k = fs->ptrs_per_block;
    int r = ext2_prune(n, &n->ino.sibp, 1, EXT2_DIRECT_BLOCK_POINTERS, new_start);
    if (r != 0) { LOG(WARN, "prune(sibp) failed with error %d\n", r); ext2_markError(fs); return r; }
    r = ext2_prune(n, &n->ino.dibp, 2, EXT2_DIRECT_BLOCK_POINTERS + k, new_start);
    if (r != 0) { LOG(WARN, "prune(dibp) failed with error %d\n", r); ext2_markError(fs); return r; }
    r = ext2_prune(n, &n->ino.tibp, 3, EXT2_DIRECT_BLOCK_POINTERS + k + (k*k), new_start);
    if (r != 0) { LOG(WARN, "prune(tibp) failed with error %d\n", r); ext2_markError(fs); return r; }
    
    // reset last_alloc, it should come out of what we just freed
    n->last_alloc = 0;
    
_finish:
    ext2_setSize(fs, &n->ino, new_size);
    n->dirty = true;
    return 0;
}

// stupid helper for ext2_readlink
ssize_t ext2_nodeReadBytes(ext2_node_t *n, loff_t off, size_t size, char *buffer) {
    ext2_t *fs = n->fs;
    
    size_t done = 0;
    while (done < size) {
        uint32_t lblk = (uint32_t)(off >> fs->blkshift);
        size_t blkoff = (size_t)(off & (fs->blksize - 1));
        size_t chunk = min(fs->blksize - blkoff, size - done);

        ext2_extent_t ext;
        int r = ext2_map(n, lblk, 1, false, &ext);
        if (r != 0) return r;
        if (!ext.pblk) {
            memset(buffer + done, 0, chunk);
        } else {
            ext2_buffer_t *buf;
            char *p = ext2_blockGet(fs, ext.pblk, &buf);
            if (!p) return -EIO;

            memcpy(buffer + done, p + blkoff, chunk);
            ext2_bcacheFinish(fs, buf);
        }

        off += chunk;
        done += chunk;
    }

    return (ssize_t)size;
}

// stupid helper for ext2_symlink
ssize_t ext2_nodeWriteBytes(ext2_node_t *n, loff_t off, size_t size, char *buffer) {
    ext2_t *fs = n->fs;
    size_t done = 0;

    while (done < size) {
        uint32_t lblk = (uint32_t)(off >> fs->blkshift);
        size_t blkoff = (size_t)(off & (fs->blksize - 1));
        size_t chunk = min(fs->blksize - blkoff, size - done);

        ext2_extent_t ext;
        int r = ext2_map(n, lblk, 1, false, &ext);
        if (r != 0) return r;

        bool fresh = (ext.pblk == 0);
        if (fresh) {
            r = ext2_map(n, lblk, 1, true, &ext);
            if (r != 0) return r;
        }

        if (!ext.pblk) return -EIO;

        ext2_buffer_t *buf;
        char *p;

        if (fresh || chunk == fs->blksize) {
            p = ext2_blockGetZero(fs, ext.pblk, &buf);
            if (!p) return -EIO;
            if (fresh && chunk != fs->blksize) memset(p, 0, fs->blksize);
        } else {
            p = ext2_blockGet(fs, ext.pblk, &buf);
            if (!p) return -EIO;
        }

        memcpy(p + blkoff, buffer + done, chunk);
        buf->dirty = true;
        ext2_bcacheFinish(fs, buf);

        off += chunk;
        done += chunk;
    }

    if ((uint64_t)off > ext2_size(fs, &n->ino)) {
        ext2_setSize(fs, &n->ino, (uint64_t)off);
        n->dirty = true;
    }

    return (ssize_t)size;
}
