/**
 * @file drivers/fs/ext2/inode.c
 * @brief Inode management
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
#include <kernel/mm/slab.h>
#include <kernel/debug.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "DRIVER:EXT2:INODE", __VA_ARGS__)

static loff_t ext2_inodeOffset(ext2_t *fs, ino_t ino) {
    assert(ino >= 1 && ino <= fs->sb.inode_count);

    uint32_t grp = EXT2_INO_GROUP(fs, ino);
    assert(grp < fs->ngroups);

    return EXT2_BLOCK_TO_DISK(fs, EXT2_GROUP(fs,grp)->bg.inode_table) + ((loff_t)EXT2_INO_INDEX(fs, ino) * fs->inosize);
}

int ext2_inodeRead(ext2_t *fs, ino_t ino, ext2_inode_t *out) {
    loff_t ino_off = ext2_inodeOffset(fs, ino);

    ext2_buffer_t *buffer = ext2_bcacheGet(fs, ino_off / PAGE_SIZE);
    if (!buffer) return -EIO; // TODO error code passing

    memcpy(out, buffer->data + (ino_off & (PAGE_SIZE-1)), sizeof(ext2_inode_t));
    ext2_bcacheFinish(fs, buffer);
    return 0;
}

int ext2_inodeWrite(ext2_t *fs, ino_t ino, ext2_inode_t *out) {
    loff_t ino_off = ext2_inodeOffset(fs, ino);

    ext2_buffer_t *buffer = ext2_bcacheGet(fs, ino_off / PAGE_SIZE);
    if (!buffer) return -EIO; // TODO error code passing

    memcpy(buffer->data + (ino_off & (PAGE_SIZE-1)), out, sizeof(ext2_inode_t));
    buffer->dirty = true;
    ext2_bcacheFinish(fs, buffer);
    return 0;
}

int ext2_inodeClear(ext2_t *fs, ino_t ino) {
    assert(0 && "ext2_inodeClear NYI");
}
