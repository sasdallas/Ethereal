/**
 * @file drivers/fs/ext2/dir.c
 * @brief Directory management
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
#include <kernel/mm/alloc.h>
#include <kernel/debug.h>
#include <string.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "DRIVER:EXT2:DIR", __VA_ARGS__)

/* Smallest a dirent can be */
#define DIRENT_MIN(namelen) ALIGN_UP(8 + (namelen), 4)

/* ext2 dirent to VFS type */
static const uint32_t ext2_vfs_type[] = {
    DT_REG, DT_REG, DT_DIR, DT_CHR,
    DT_BLK, DT_FIFO, DT_SOCK, DT_LNK,
};

static const uint8_t ext2_dirent_type[] = {
    1, 2, 3, 4, 5, 7, 6
};

static char *ext2_dirBlock(ext2_node_t *node, uint32_t lblk, bool alloc, ext2_buffer_t **bufout) {
    ext2_t *fs = node->fs;

    ext2_extent_t ext;
    int r = ext2_map(node, lblk, 1, false, &ext);
    if (r != 0) return NULL;

    if (!ext.pblk) {
        if (!alloc) return NULL;

        r = ext2_map(node, lblk, 1, true, &ext);
        if (r != 0 || ext.pblk == 0) return NULL;

        char *p = ext2_blockGetZero(fs, ext.pblk, bufout);
        if (!p) return NULL;

        memset(p, 0, fs->blksize);
        (*bufout)->dirty = true;

        return p;
    }

    return ext2_blockGet(fs, ext.pblk, bufout);
}

int ext2_dirFind(ext2_node_t *n, char *name, ino_t *out) {
    ext2_t *fs = n->fs;
    uint64_t size = ext2_size(fs, &n->ino);
    uint32_t nblocks = (uint32_t)EXT2_TO_BLOCKS(fs, size);

    // max of 255 per name
    size_t nlen = strlen(name);
    if (nlen > 255) return -ENAMETOOLONG;

    for (uint32_t lblk = 0; lblk < nblocks; lblk++) {
        ext2_buffer_t *buf;
        char *blk = ext2_dirBlock(n, lblk, false, &buf);
        if (!blk) continue;

        size_t off = 0;
        while (off + sizeof(ext2_dirent_t) <= fs->blksize) {
            ext2_dirent_t *dent = (ext2_dirent_t*)(blk + off);

            if (dent->size < DIRENT_MIN(1) || off + dent->size > fs->blksize) {
                break;
            }

            if (dent->inode) {
                if (dent->name_length == nlen && !memcmp(dent->name, name, nlen)) {
                    *out = (ino_t)dent->inode;
                    ext2_bcacheFinish(fs, buf);
                    return 0;
                }
            }

            off += dent->size;
        }

        ext2_bcacheFinish(fs, buf);
    }

    return -ENOENT;
}

int ext2_dirIterate(ext2_node_t *node, vfs_dir_context_t *ctx) {
    ext2_t *fs = node->fs;
    uint64_t size = ext2_size(fs, &node->ino);
    uint32_t nblocks = (uint32_t)EXT2_TO_BLOCKS(fs, size);

    uint32_t start_lblk = 0;
    uint32_t start_off = 0;
    unsigned index = 0;

    // check the dcursor cache first
    spinlock_acquire(&node->dcursor.lock);
    if (node->dcursor.valid && node->dcursor.pos == ctx->dirpos && node->dcursor.lblk < nblocks) {
        start_lblk = node->dcursor.lblk;
        start_off = node->dcursor.off;
        index = node->dcursor.pos;
    }
    spinlock_release(&node->dcursor.lock);

    // lesgo
    for (uint32_t lblk = start_lblk; lblk < nblocks; lblk++) {
        ext2_buffer_t *buf;
        char *blk = ext2_dirBlock(node, lblk, false, &buf);
        if (!blk) continue;

        size_t off = (lblk == start_lblk) ? start_off : 0;
        while (off + sizeof(ext2_dirent_t) <= fs->blksize) {
            ext2_dirent_t *dent = (ext2_dirent_t*)(blk + off);

            if (dent->size < DIRENT_MIN(1) || off + dent->size > fs->blksize) {
                break;
            }

            if (dent->inode) {
                if (index == ctx->dirpos) {
                    size_t nlen = min((size_t)dent->name_length, (size_t)(NAME_MAX - 1));
                    memcpy(ctx->name, dent->name, nlen);
                    ctx->name[nlen] = 0;
                    ctx->ino = dent->inode;
                    ctx->type = ext2_vfs_type[dent->type & 7];

                    // update dcursor
                    spinlock_acquire(&node->dcursor.lock);
                    node->dcursor.valid = true;
                    node->dcursor.pos = index + 1;
                    node->dcursor.lblk = lblk;
                    node->dcursor.off = (uint32_t)(off + dent->size);
                    spinlock_release(&node->dcursor.lock);

                    ext2_bcacheFinish(fs, buf);
                    return 0;
                }

                index++;
            }

            off += dent->size;
        }

        ext2_bcacheFinish(fs, buf);
    }

    return 1;
}


int ext2_dirInsert(ext2_node_t *n, char *name, ino_t ino, vfs_ino_type_t type) {
    ext2_t *fs = n->fs;
    size_t nlen = strlen(name);
    if (nlen > 255) return -ENAMETOOLONG;

    size_t needed = DIRENT_MIN(nlen);
    uint64_t dir_size = ext2_size(fs, &n->ino);
    
    uint32_t nblocks = EXT2_TO_BLOCKS(fs, dir_size);
    uint32_t hint = (n->dcursor.ins_hint < nblocks) ? n->dcursor.ins_hint : 0;

    // lets go
    // TODO this is kinda slow
    for (uint32_t scanned = 0; scanned < nblocks; scanned++) {
        uint32_t lblk = (hint + scanned) % nblocks;

        // check if this block has any free directory entries
        ext2_buffer_t *buf;
        char *block = ext2_dirBlock(n, lblk, false, &buf);
        if (block == NULL) {
            continue;
        }

        size_t off = 0;
        while (off + sizeof(ext2_dirent_t) <= fs->blksize) {
            ext2_dirent_t *dent = (ext2_dirent_t*)(block + off);

            if (dent->size < DIRENT_MIN(1) || off + dent->size > fs->blksize) {
                break;
            }

            // if the dent has no inode, then it is free. if it does, it still might be padding towards the end
            size_t keep = dent->inode ? DIRENT_MIN(dent->name_length) : 0;
            if (dent->size - keep >= needed) {

                ext2_dirent_t *tgt;
                if (keep != 0) {
                    // this dent is padded, take over after
                    uint16_t total = dent->size;
                    dent->size = (uint16_t)keep;
                    tgt = (ext2_dirent_t*)(block + off + keep);
                    tgt->size = (uint16_t)(total - keep);
                } else {
                    // its fully free
                    tgt= dent;
                }

                tgt->inode = (uint32_t)ino;
                tgt->name_length = nlen;
                tgt->type = ext2_dirent_type[type];
                memcpy(tgt->name, name, nlen);

                
                // invalidate read cursor
                spinlock_acquire(&n->dcursor.lock);
                n->dcursor.valid = false;
                spinlock_release(&n->dcursor.lock);

                buf->dirty = true;
                ext2_bcacheFinish(fs, buf);

                n->dcursor.ins_hint = lblk;
                return 0;
            }

            off += dent->size;
        }

        ext2_bcacheFinish(fs, buf);
    }

    // there was no room
    ext2_buffer_t *buf;
    char *block = ext2_dirBlock(n, nblocks, true, &buf);
    if (!block) return -ENOSPC;

    // create the dent
    ext2_dirent_t *dent = (ext2_dirent_t*)block;
    dent->inode = (uint32_t)ino;
    dent->size = (uint16_t)fs->blksize;
    dent->name_length = nlen;
    dent->type = ext2_dirent_type[type];
    memcpy(dent->name, name, nlen);

    // invalidate read cursor
    spinlock_acquire(&n->dcursor.lock);
    n->dcursor.valid = false;
    spinlock_release(&n->dcursor.lock);

    // mark buffer
    buf->dirty = true;
    ext2_bcacheFinish(fs, buf);

    // update inode
    n->dirty = true;
    n->dcursor.ins_hint = nblocks;
    ext2_setSize(fs, &n->ino, (nblocks + 1) << fs->blkshift);
    return 0;
}

int ext2_dirRemove(ext2_node_t *n, char *name, ino_t ino) {
    ext2_t *fs = n->fs;
    size_t nlen = strlen(name);
    uint32_t nblocks = EXT2_TO_BLOCKS(fs, ext2_size(fs, &n->ino));

    for (uint32_t lblk = 0; lblk < nblocks; lblk++) {
        ext2_buffer_t *buf;
        char *block = ext2_dirBlock(n, lblk, false, &buf);
        if (!block) return -EIO;

        size_t off = 0;
        ext2_dirent_t *prev = NULL;
        while (off + sizeof(ext2_dirent_t) <= fs->blksize) {
            ext2_dirent_t *dent = (ext2_dirent_t*)(block + off);
            if (dent->size < DIRENT_MIN(1) || off + dent->size > fs->blksize) {
                ext2_bcacheFinish(fs, buf);
                return -EIO;
            }

            if (dent->inode == ino && dent->name_length == nlen && !memcmp(dent->name, name, nlen)) {
                if (prev) prev->size += dent->size;
                else dent->inode = 0;

                spinlock_acquire(&n->dcursor.lock);
                n->dcursor.valid = false;
                spinlock_release(&n->dcursor.lock);
                buf->dirty = true;
                ext2_bcacheFinish(fs, buf);
                return 0;
            }

            prev = dent;
            off += dent->size;
        }

        ext2_bcacheFinish(fs, buf);
    }

    return -ENOENT;
}

int ext2_dirReplace(ext2_node_t *n, char *name, ino_t old_ino, ino_t new_ino, vfs_ino_type_t type) {
    ext2_t *fs = n->fs;
    size_t nlen = strlen(name);
    uint32_t nblocks = EXT2_TO_BLOCKS(fs, ext2_size(fs, &n->ino));

    for (uint32_t lblk = 0; lblk < nblocks; lblk++) {
        ext2_buffer_t *buf;
        char *block = ext2_dirBlock(n, lblk, false, &buf);
        if (!block) return -EIO;

        size_t off = 0;
        while (off + sizeof(ext2_dirent_t) <= fs->blksize) {
            ext2_dirent_t *dent = (ext2_dirent_t*)(block + off);
            if (dent->size < DIRENT_MIN(1) || off + dent->size > fs->blksize) {
                ext2_bcacheFinish(fs, buf);
                return -EIO;
            }

            if (dent->inode == old_ino && dent->name_length == nlen && !memcmp(dent->name, name, nlen)) {
                dent->inode = (uint32_t)new_ino;
                dent->type = ext2_dirent_type[type];
                buf->dirty = true;
                ext2_bcacheFinish(fs, buf);
                return 0;
            }

            off += dent->size;
        }

        ext2_bcacheFinish(fs, buf);
    }

    return -ENOENT;
}

int ext2_dirEmpty(ext2_node_t *n, bool *empty) {
    ext2_t *fs = n->fs;
    uint32_t nblocks = EXT2_TO_BLOCKS(fs, ext2_size(fs, &n->ino));
    if (!nblocks) return -EIO;
    *empty = true;

    for (uint32_t lblk = 0; lblk < nblocks; lblk++) {
        ext2_buffer_t *buf;
        char *block = ext2_dirBlock(n, lblk, false, &buf);
        if (!block) return -EIO;

        size_t off = 0;
        while (off + sizeof(ext2_dirent_t) <= fs->blksize) {
            ext2_dirent_t *dent = (ext2_dirent_t*)(block + off);
            if (dent->size < DIRENT_MIN(1) || off + dent->size > fs->blksize) {
                ext2_bcacheFinish(fs, buf);
                return -EIO;
            }

            if (dent->inode) {
                bool dot = dent->name_length == 1 && dent->name[0] == '.';
                bool dotdot = dent->name_length == 2 && dent->name[0] == '.' && dent->name[1] == '.';
                if (!dot && !dotdot) {
                    *empty = false;
                    ext2_bcacheFinish(fs, buf);
                    return 0;
                }
            }

            off += dent->size;
        }

        ext2_bcacheFinish(fs, buf);
    }

    return 0;
}

int ext2_dirSetParent(ext2_node_t *n, ino_t parent) {
    ext2_t *fs = n->fs;
    ext2_buffer_t *buf;
    char *block = ext2_dirBlock(n, 0, false, &buf);
    if (!block) return -EIO;

    size_t off = 0;
    while (off + sizeof(ext2_dirent_t) <= fs->blksize) {
        ext2_dirent_t *dent = (ext2_dirent_t*)(block + off);
        if (dent->size < DIRENT_MIN(1) || off + dent->size > fs->blksize) {
            ext2_bcacheFinish(fs, buf);
            return -EIO;
        }

        if (dent->inode && dent->name_length == 2 && dent->name[0] == '.' && dent->name[1] == '.') {
            dent->inode = (uint32_t)parent;
            buf->dirty = true;
            ext2_bcacheFinish(fs, buf);
            return 0;
        }

        off += dent->size;
    }

    ext2_bcacheFinish(fs, buf);
    return -EIO;
}

