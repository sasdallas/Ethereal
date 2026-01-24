/**
 * @file hexahedron/fs/initfs.c
 * @brief Initial filesystem
 * 
 * The initial filesystem is created when the kernel boots up initially.
 * It is used to mount /device/, /kernel/, and other internal structures before the root filesystem is mounted.
 * @c vfs_pivot can then be used to change the root inode (and migrate the filesystems)
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
#include <kernel/debug.h>
#include <kernel/init.h>
#include <limits.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "FS:INITFS", __VA_ARGS__)

static int initfs_lookup(vfs_inode_t *inode, char *name, vfs_inode_t **inode_output);
static int initfs_mkdir(vfs_inode_t *parent, char *name, mode_t mode, vfs_inode_t **ino_output);
static int initfs_get_entries(vfs_file_t *file, vfs_dir_context_t *ctx);
static int initfs_mount(vfs2_filesystem_t *filesystem, vfs_mount_t *mount_dst, char *src, unsigned long flags, void *data);
static int initfs_unmount(vfs_mount_t *mount);
static int initfs_destroy(vfs_inode_t *inode);

// initfs does not support create
typedef struct initfs_node {
    mutex_t lck;
    char name[NAME_MAX];
    vfs_inode_t *inode_link;
    struct initfs_node *children;
    struct initfs_node *next;
    struct initfs_node *parent;
} initfs_node_t;

vfs_inode_ops_t initfs_ops = {
    .create = NULL,
    .lookup = initfs_lookup,
    .mkdir = initfs_mkdir,
    .link = NULL,
    .symlink = NULL,
    .rmdir = NULL,
    .unlink = NULL,
    .readlink = NULL,
    .getattr = NULL,
    .setattr = NULL,
    .truncate = NULL,
    .destroy = initfs_destroy
};

vfs_file_ops_t initfs_f_ops = {
    .open = NULL,
    .read = NULL,
    .write = NULL,
    .ioctl = NULL,
    .close = NULL,
    .get_entries = initfs_get_entries,
};

vfs_mount_ops_t initfs_m_ops = {
    .write_inode = NULL,
    .unmount = initfs_unmount
};

vfs2_filesystem_t initfs_filesystem = {
    .name = "initfs",
    .flags = 0,
    .fs_mounts = NULL,
    .mount = initfs_mount
};

/* Init filesystem root */
initfs_node_t *initfs_root = NULL;

/**
 * @brief initfs lookup
 */
static int initfs_lookup(vfs_inode_t *inode, char *name, vfs_inode_t **inode_output) {
    initfs_node_t *n = (initfs_node_t*)inode->priv;
    
    // Look for the child
    initfs_node_t *child = n->children;
    while (child) {
        if (!strncmp(name, child->name, NAME_MAX)) {
            *inode_output = child->inode_link;
            return 0;
        }

        child = child->next;
    }

    return -ENOENT;
}

/**
 * @brief initfs mkdir
 */
static int initfs_mkdir(vfs_inode_t *parent, char *name, mode_t mode, vfs_inode_t **ino_output) {
    initfs_node_t *n = (initfs_node_t*)parent->priv;

    // Let's add a child
    mutex_acquire(&n->lck);

    // Allocate the child
    initfs_node_t *child = kzalloc(sizeof(initfs_node_t));
    if (!child) { mutex_release(&n->lck); return -ENOMEM; }
    strncpy(child->name, name, NAME_MAX);
    child->parent = n;
    MUTEX_INIT(&child->lck);

    // And its inode
    vfs_inode_t *child_inode = vfs2_inode();
    if (!child) { kfree(child); mutex_release(&n->lck); return -ENOMEM; }
    child_inode->ops = &initfs_ops;
    child_inode->f_ops = &initfs_f_ops;
    child_inode->flags = INODE_FLAG_DEFAULT;
    child_inode->mount = parent->mount;
    child_inode->priv = (void*)child;
    child_inode->attr.atime = child_inode->attr.mtime = child_inode->attr.ctime = VFS_NOW();
    child_inode->attr.nlink = 2;
    child_inode->attr.mode = mode;
    child_inode->attr.type = VFS_DIRECTORY;
    child_inode->attr.ino = vfs_getNextInode();
    child->inode_link = child_inode;

    // Now, safely add the child
    n->inode_link->attr.nlink++;
    child->next = n->children;
    n->children = child;
    mutex_release(&n->lck);
    
    *ino_output = child_inode;
    return 0;
}

/**
 * @brief initfs get entries
 */
static int initfs_get_entries(vfs_file_t *file, vfs_dir_context_t *ctx) {
    initfs_node_t *n = (initfs_node_t*)file->inode->priv;
    if (ctx->dirpos == 0) {
        ctx->name = ".";
        ctx->ino = file->inode->attr.ino;
        ctx->type = DT_DIR;
        return 0;
    } else if (ctx->dirpos == 1) {
        ctx->name = "..";
        if (n->parent) ctx->ino = n->parent->inode_link->attr.ino;
        else ctx->ino = file->inode->attr.ino;
        ctx->type = DT_DIR;
        return 0;
    }
    
    int i = ctx->dirpos - 2;

    mutex_acquire(&n->lck);

    int j = 0;
    initfs_node_t *child = n->children;
    while (child) {
        if (i == j) {
            mutex_release(&n->lck);
            ctx->name = child->name;
            ctx->ino = child->inode_link->attr.ino;
            ctx->type = DT_DIR;
            return 0;
        }

        j++;
        child = child->next;
    }

    mutex_release(&n->lck);
    return 1; // Directory is finished
}

/**
 * @brief initfs mount
 */
static int initfs_mount(vfs2_filesystem_t *filesystem, vfs_mount_t *mount_dst, char *src, unsigned long flags, void *data) {
    // If needed start the initial filesystem.
    if (!initfs_root) {
        initfs_node_t *root_node = kzalloc(sizeof(initfs_node_t));
        MUTEX_INIT(&root_node->lck);
        root_node->name[0] = '/';
        initfs_root = root_node;
    
        // Create the root inode
        vfs_inode_t *root = vfs2_inode();
        if (!root) return -ENOMEM;

        root->ops = &initfs_ops;
        root->f_ops = &initfs_f_ops;
        root->flags = INODE_FLAG_DEFAULT;
        root->attr.mode = 0755; // rwxr-xr-x
        root->attr.ino = vfs_getNextInode();
        root->attr.gid = root->attr.uid = 0;
        root->attr.type = VFS_DIRECTORY;
        root->attr.nlink = 2;
        root->attr.atime = root->attr.mtime = root->attr.ctime = VFS_NOW();
        root->priv = (void*)initfs_root;
        initfs_root->inode_link = root;
    }

    mount_dst->root = initfs_root->inode_link;
    mount_dst->ops = &initfs_m_ops;
    return 0;
}

/**
 * @brief initfs destroy
 */
static int initfs_destroy(vfs_inode_t *inode) {
    kfree(inode->priv);
    return 0;
}


/**
 * @brief initfs unmount
 */
static int initfs_unmount(vfs_mount_t *mount) {
    return 0;
}

/**
 * @brief initfs init
 */
int initfs_init() {
    vfs_register(&initfs_filesystem);
    return 0;
}

FS_INIT_ROUTINE(initfs, INIT_FLAG_DEFAULT, initfs_init);