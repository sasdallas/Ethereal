/**
 * @file drivers/fs/ext2/alloc.c
 * @brief EXT2 block allocator
 * 
 * Pretty simple, just can allocate many blocks at once.
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#include "ext2.h"
#include <kernel/debug.h>
#include <kernel/mm/cache.h>
#include <string.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "DRIVER:EXT2:ALLOC", __VA_ARGS__)

static size_t ext2_scanBitmap(uint64_t *bm, size_t bmsize, uint32_t hint, size_t wanted, uint32_t *out) {
    size_t i = hint;
    size_t max_run = 0;
    size_t best_start = hint;
    size_t current_run = 0;
    size_t current_start = hint;
    while (i < bmsize) {
        size_t word_idx = i / 64;
        size_t bit = i % 64;

        uint64_t word = bm[word_idx];
        if (!bit && word == ~0ULL) {
            current_run = 0;
            i += 64;
            continue;
        }

        if ((word & (1ULL << bit)) == 0) {
            if (current_run == 0) current_start = i;
            current_run += 1;

            if (current_run >= wanted) {
                *out = current_start;
                return (size_t)current_run;
            }
        } else {
            if (current_run > max_run) {
                max_run = current_run;
                best_start = current_start;
            }

            current_run = 0;
        }

        i += 1;
    }

    if (current_run > max_run) {
        max_run = current_run;
        best_start = current_start;
    }

    *out = max_run ? best_start : 0;
    return max_run;
}


static int ext2_maybeLoadGroup(ext2_t *fs, uint32_t g, bool load_inode) {
    ext2_group_t *grp = EXT2_GROUP(fs, g);

    // Check if already loaded
    GROUP_START_READ(fs, g);
    if ((load_inode ? grp->inobm : grp->blkbm) != NULL) {
        GROUP_FINISH_READ(fs, g);
        return 0;
    }
    GROUP_FINISH_READ(fs, g);

    // No we have to load it
    uint32_t *buf = kmalloc(fs->blksize);

    uint32_t blk = load_inode ? grp->bg.inode_usage_bitmap : grp->bg.block_usage_bitmap;
    int r = ext2_blockRead(fs, blk, buf);
    if (r != 0) {
        kfree(buf);
        return r;
    }


    // did we lose the race
    GROUP_START_WRITE(fs, g);
    uint32_t **buffer = load_inode ? &grp->inobm : &grp->blkbm;
    if (*buffer) {
        GROUP_FINISH_WRITE(fs, g);
        kfree(buf);
        return 0;
    }

    *buffer = buf;
    GROUP_FINISH_WRITE(fs, g);

    return 0;
}

static ssize_t ext2_allocGroupBlock(ext2_t *fs, uint32_t g, uint32_t hint, size_t want, uint32_t *start) {
    ext2_group_t *grp = EXT2_GROUP(fs, g);
    if (grp->bg.free_blocks == 0) {
        return 0;
    }

    int r = ext2_maybeLoadGroup(fs, g, false);
    if (r != 0) {
        return r;
    }

    // its theoretically possible that a group can have less than bg_block_count members if filling up
    // this is the case for the last group usually
    uint32_t avail = fs->sb.block_count - (fs->sb.start_block_number + (g * fs->sb.bg_block_count));
    uint32_t blocks = min(avail, fs->sb.bg_block_count);
    if (hint >= blocks) hint = 0;

    // check from the hint
    GROUP_START_WRITE(fs, g);

    if (grp->bg.free_blocks == 0) {
        GROUP_FINISH_WRITE(fs, g);
        return 0;
    }

    uint32_t best_start = 0;
    size_t best_len = 0;
    best_len = ext2_scanBitmap((uint64_t*)grp->blkbm, blocks, hint, want, &best_start);
    if (best_len < want && hint != 0) {
        // try again, scanning this time for the first half. only override if there's more
        uint32_t front_start = 0;
        size_t front_len = ext2_scanBitmap((uint64_t*)grp->blkbm, hint, 0, want, &front_start);
        if (front_len > best_len) {
            best_len = front_len;
            best_start = front_start;
        }
    }

    if (best_len == 0) {
        LOG(WARN, "Group %u reports %u free blocks, but bitmap has none (blocks %u, hint %u)\n", g, grp->bg.free_blocks, blocks, hint);
        GROUP_FINISH_WRITE(fs, g);
        ext2_markError(fs);
        return 0;
    }

    if (best_len > want) best_len = want;

    uint32_t *bm = grp->blkbm;
    for (size_t i = best_start; i < best_start + best_len; i++) {
        bm[i / 32] |= (1UL << (i % 32));        
    }

    grp->bg.free_blocks -= best_len;
    grp->bg_dirty = true;
    grp->blkbm_dirty = true;
    grp->hint = best_start + best_len;

    GROUP_FINISH_WRITE(fs, g);

    *start = (fs->sb.start_block_number + (g * fs->sb.bg_block_count)) + best_start;
    return (ssize_t)best_len;
}

int ext2_alloc(ext2_t *fs, uint32_t preferred, size_t want, uint32_t *start, size_t *got) {
    uint32_t first = fs->alloc_hint;
    uint32_t hint = 0;

    if (preferred != 0) {
        first = EXT2_BLK_GROUP(fs, preferred);
        hint = EXT2_BLK_INDEX(fs, preferred);
    }

    if (first >= fs->ngroups) {
        first = 0;
    }

    // rotate around the groups list
    for (uint32_t i = 0; i < fs->ngroups; i++) {
        uint32_t g = (first + i) % fs->ngroups;

        uint32_t ghint = (i == 0) ? hint : fs->groups[g].hint;
        ssize_t n = ext2_allocGroupBlock(fs, g, ghint, want, start);
        if (n < 0) {
            // error
            return (int)n;
        }

        if (n == 0) {
            // got nothing
            continue;
        }

        // yay
        mutex_acquire(&fs->sb_lock);
        fs->sb.unallocated_blocks -= n;
        fs->sb_dirty = true;
        mutex_release(&fs->sb_lock);

        fs->alloc_hint = g;
        *got = (size_t)n;

        cache_requestSync();
        return 0;
    }

    return -ENOSPC;
}

// b_group is the group to prefer allocation in. typically its the same as the parent driectory
int ext2_allocInode(ext2_t *fs, uint32_t b_group, bool dir, ino_t *out) {
    for (uint32_t i = 0; i < fs->ngroups; i++) {
        uint32_t g = (b_group + i) % fs->ngroups;
        ext2_group_t *grp = EXT2_GROUP(fs, g);

        // !!! racey but doesn't really matter
        if (grp->bg.free_inodes == 0) {
            continue;
        }

        int r = ext2_maybeLoadGroup(fs, g, true);
        if (r != 0) {
            return r;
        }

        GROUP_START_WRITE(fs, g);
        if (grp->bg.free_inodes == 0) {
            GROUP_FINISH_WRITE(fs, g);
            continue;
        }

        // TODO: grp->inohint
        uint32_t best_start;
        size_t best_len = ext2_scanBitmap((uint64_t*)grp->inobm, fs->sb.bg_ino_count, 0, 1, &best_start);
        if (best_len == 0) {
            // it lies
            GROUP_FINISH_WRITE(fs, g);
            ext2_markError(fs);
            continue;
        }

        // update and dirty group
        grp->inobm[best_start / 32] |= (1UL << (best_start % 32));
        grp->bg.free_inodes -= 1;
        if (dir) grp->bg.directories += 1;
        grp->inobm_dirty = true;
        grp->bg_dirty = true;

        // finished
        GROUP_FINISH_WRITE(fs, g);

        // update superblock
        mutex_acquire(&fs->sb_lock);
        fs->sb.unallocated_inodes -= 1;
        fs->sb_dirty = true;
        mutex_release(&fs->sb_lock);

        *out = (ino_t)(best_start + (g * fs->sb.bg_ino_count) + 1);
        cache_requestSync();
        return 0;
    }

    return -ENOSPC;
}

int ext2_freeBlocks(ext2_t *fs, uint32_t start, size_t count) {
    size_t rem = count;
    while (rem) {
        uint32_t grp_index = EXT2_BLK_GROUP(fs, start);
        uint32_t grp_start = (fs->sb.start_block_number + grp_index * fs->sb.bg_block_count);
        uint32_t free_index = (start - grp_start);
        size_t chunk = min(rem, (size_t)min(fs->sb.bg_block_count - free_index, fs->sb.block_count - start));
        ext2_group_t *grp = EXT2_GROUP(fs, grp_index);

        // load group 
        int r = ext2_maybeLoadGroup(fs, grp_index, false);
        if (r != 0) {
            ext2_markError(fs);
            return r;
        }

        // free loop
        GROUP_START_WRITE(fs, grp_index);
        for (unsigned i = free_index; i < free_index+chunk; i++) {
            grp->blkbm[i / 32] &= ~(1UL << (i % 32));
        }

        grp->bg.free_blocks += chunk;
        grp->bg_dirty = true;
        grp->blkbm_dirty = true;
        GROUP_FINISH_WRITE(fs, grp_index);

        rem -= chunk;
        start += chunk;
    }
    
    mutex_acquire(&fs->sb_lock);
    fs->sb.unallocated_blocks += count;
    fs->sb_dirty = true;
    mutex_release(&fs->sb_lock);

    cache_requestSync();
    return 0;
}

int ext2_freeInode(ext2_t *fs, ino_t ino, bool dir) {
    uint32_t g = EXT2_INO_GROUP(fs, ino);
    ext2_group_t *grp = EXT2_GROUP(fs, g);

    int r = ext2_maybeLoadGroup(fs, g, true);
    if (r != 0) {
        ext2_markError(fs);
        return r;
    }

    ino_t ino_offset = ino - (g * fs->sb.bg_ino_count + 1);

    // unset + update
    GROUP_START_WRITE(fs, g);
    grp->inobm[ino_offset / 32] &= ~(1 << (ino_offset % 32));
    grp->bg.free_inodes += 1;
    if (dir) grp->bg.directories -= 1;
    grp->bg_dirty = true;
    grp->inobm_dirty = true;
    GROUP_FINISH_WRITE(fs, g);

    mutex_acquire(&fs->sb_lock);
    fs->sb.unallocated_inodes += 1;
    fs->sb_dirty = true;
    mutex_release(&fs->sb_lock);

    cache_requestSync();
    return 0;
}
