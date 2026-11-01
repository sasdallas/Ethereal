/**
 * @file drivers/fs/ext2/ext2.c
 * @brief EXT2 driver
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
#include <kernel/loader/driver.h>
#include <kernel/fs/vfs_new.h>
#include <kernel/debug.h>
#include <string.h>
#include <assert.h>

/* EXT2 filesystem */
static int ext2_sync(vfs_mount_t *mount);
static int ext2_write_inode(vfs_inode_t *inode);
vfs_mount_ops_t ext2_mount_ops = {
    .statvfs = NULL,
    .sync = ext2_sync,
    .unmount = NULL,
    .write_inode = ext2_write_inode
};

static int ext2_mount(vfs_filesystem_t *fs, vfs_mount_t *mount, char *src, unsigned long flags, void *data);
vfs_filesystem_t ext2_fs = {
    .name = "ext2",
    .flags = 0,
    .fs_mounts = NULL,
    .mount = ext2_mount
};

/* Log method */
#define LOG(status, ...) dprintf_module(status, "FS:EXT2", __VA_ARGS__)

/**
 * @brief EXT2 mount method
 */
static int ext2_mount(vfs_filesystem_t *fs, vfs_mount_t *mount, char *src, unsigned long flags, void *data) {
    // Try to open source
    vfs_file_t *backing;
    if (vfs_open(src, O_RDWR, &backing) != 0) {
        LOG(INFO, "Device %s not found\n", src);
        return -ENOTBLK;
    }

    // Backing is ready, now read the superblock
    ext2_sb_t sb;
    ssize_t r = vfs_read(backing, EXT2_SB_OFFSET, sizeof(ext2_sb_t), (char*)&sb);
    if (r != sizeof(ext2_sb_t)) {
        if (r > 0) return -EINVAL;
        vfs_close(backing);
        return r;
    }

    // Check signature
    if (sb.signature != EXT2_SB_SIGNATURE) {
        LOG(INFO, "Invalid EXT2 signature (0x%x)\n", sb.signature);
        vfs_close(backing);
        return -EINVAL;
    }

    // Confirmed EXT2 filesystem, initialize it
    ext2_t *ext2 = kzalloc(sizeof(ext2_t));
    MUTEX_INIT(&ext2->sb_lock);
    memcpy(&ext2->sb, &sb, sizeof(ext2_sb_t));
    ext2->mount = mount;
    ext2->backing = backing;

    ext2->blksize = (size_t)(1024 << ext2->sb.block_size);
    assert(ext2->blksize <= PAGE_SIZE && "unsupported");

    if (sb.ver_major > 0) {
        ext2->inosize = sb.extended.inode_struct_size;
    } else {
        ext2->inosize = 128;
    }

    ext2->blkshift = 10 + sb.block_size;
    ext2->blocks_per_page = PAGE_SIZE / ext2->blksize;
    ext2->ptrs_per_block = ext2->blksize / sizeof(uint32_t);

    // Initialize block group cache
    ext2_bcacheInit(ext2);

    // Initialize group cache
    uint32_t ngroups = (sb.block_count - sb.start_block_number + sb.bg_block_count - 1) / sb.bg_block_count;
    ext2->groups = kmalloc(sizeof(ext2_group_t) * ngroups);    
    ext2->ngroups = ngroups;

    int error = 0;

    for (unsigned i = 0; i < ngroups; i++) {
        ext2_group_t *grp = EXT2_GROUP(ext2, i);

        RWSEM_INIT(&grp->sem);
        grp->bg_dirty = false;
        grp->blkbm_dirty = false;
        grp->inobm_dirty = false;
        grp->inobm = NULL;
        grp->blkbm = NULL;
        grp->hint = 0;

        // load the BGD
        if (vfs_read(backing, EXT2_BGD_READ_OFFSET(ext2, i), sizeof(ext2_bgd_t), (char*)&grp->bg) != sizeof(ext2_bgd_t)) {
            // TODO proper destruction
            LOG(ERR, "Error while attempting to read block group %d\n", i);
            error = -EINVAL;
            goto _error;
        }
    }

    LOG(INFO, "Mounted EXT2 filesystem %u blocks (blksize %u), %u groups\n", ext2->sb.block_count, ext2->blksize, ext2->ngroups);
    mount->ops = &ext2_mount_ops;
    mount->priv = (void*)ext2;

    error = ext2_rootInit(mount, ext2);
    if (error != 0) goto _error;

    return 0;

_error:    
    vfs_close(backing);
    ext2_bcacheDestroy(ext2);
    kfree(ext2->groups);
    kfree(ext2);
    return error;
}

/**
 * @brief Sync inode groups
 */
static int ext2_syncGroups(ext2_t *fs) {
    int r = 0;
    for (uint32_t g = 0; g < fs->ngroups; g++) {
        ext2_group_t *grp = EXT2_GROUP(fs, g);

        GROUP_START_WRITE(fs, g);
        if (grp->blkbm && grp->blkbm_dirty) {
            int err = ext2_blockWrite(fs,grp->bg.block_usage_bitmap, grp->blkbm);
            if (err != 0) {
                LOG(ERR, "Failed to syncronize block group %d due to error %d\n", g, err);
                r = err;
            } else {
                grp->blkbm_dirty = false;
            }
        }
        
        if (grp->inobm && grp->inobm_dirty) {
            int err = ext2_blockWrite(fs,grp->bg.inode_usage_bitmap, grp->inobm);
            if (err != 0) {
                LOG(ERR, "Failed to syncronize block group %d due to error %d\n", g, err);
                r = err;
            } else {
                grp->inobm_dirty = false;
            }
        }
        
        if (grp->bg_dirty) {
            loff_t off = EXT2_BGD_READ_OFFSET(fs, g);;
            ext2_buffer_t *b = ext2_bcacheGet(fs, off / PAGE_SIZE);

            if (b) {
                memcpy(b->data + (off & (PAGE_SIZE-1)), &grp->bg, sizeof(ext2_bgd_t));
                b->dirty = true;
                ext2_bcacheFinish(fs,b);
                grp->bg_dirty = false;
            } else {
                LOG(ERR, "bcacheGet failed on block group %d :-(\n", g);
                r = -EIO;
            }
        }
        GROUP_FINISH_WRITE(fs, g);
    }

    return r;
}

/**
 * @brief Sync superblock
 */
static int ext2_syncSuperblock(ext2_t *fs) {
    mutex_acquire(&fs->sb_lock);
    if (!fs->sb_dirty) {
        mutex_release(&fs->sb_lock);
        return 0;
    }

    // Probably better since superblock is more contended
    ext2_sb_t tmp;
    memcpy(&tmp, &fs->sb, sizeof(ext2_sb_t));
    fs->sb_dirty = false;
    mutex_release(&fs->sb_lock);

    ext2_buffer_t *b = ext2_bcacheGet(fs, (EXT2_SB_OFFSET) / PAGE_SIZE);
    if (!b) {
        LOG(ERR, "bcacheGet for superblock failed :-(\n");
        ext2_markError(fs);
        return -EIO;
    }

    memcpy(b->data + (EXT2_SB_OFFSET % PAGE_SIZE), &tmp, sizeof(ext2_sb_t));
    b->dirty = true;
    ext2_bcacheFinish(fs, b);
    return 0;
}

/**
 * @brief Sync
 */
static int ext2_sync(vfs_mount_t *mount) {
    ext2_t *fs = mount->priv;

    // first, sync all groups
    int r = 0;
    int err = ext2_syncGroups(fs);
    if (err != 0) {
        LOG(ERR, "Error syncing block groups: %d\n", err);
        r = err;
    }

    // now the superblock
    err = ext2_syncSuperblock(fs);
    if (err != 0) {
        LOG(ERR, "Error syncing superblock: %d\n", err);
        r = err;
    }

    // and finally the block cache
    err = ext2_bcacheSync(fs);
    if (err != 0) {
        LOG(ERR, "Error syncing block cache: %d\n", err);
        r = err;
    }

    return r;
}

/**
 * @brief Write inode
 */
static int ext2_write_inode(vfs_inode_t *inode) {
    ext2_node_t *n = inode->priv;
    NODE_START_WRITE(n);
    int r = ext2_flushNode(n);
    NODE_FINISH_WRITE(n);
    return r;
}

/**
 * @brief Driver init
 */
static int driver_init(int argc, char *argv[]) {
    ext2_nodeInit();
    vfs_register(&ext2_fs);
    return DRIVER_STATUS_SUCCESS;
}

/**
 * @brief Driver deinit
 */
static int driver_deinit() {
    return DRIVER_STATUS_SUCCESS;
}

struct driver_metadata driver_metadata = {
    .name = "EXT2 Driver",
    .author = "Samuel Stuart",
    .init = driver_init,
    .deinit = driver_deinit
};
