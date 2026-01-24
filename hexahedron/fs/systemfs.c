/**
 * @file hexahedron/fs/systemfs.c
 * @brief SystemFS
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/fs/systemfs.h>
#include <kernel/debug.h>
#include <kernel/init.h>
#include <kernel/mm/vmm.h>

/* Cache */
static slab_cache_t *systemfs_node_cache;

/* Root */
systemfs_node_t *systemfs_root;

/* Protos */
int systemfs_mount(vfs2_filesystem_t *filesystem, vfs_mount_t *mount_dst, char *src, unsigned long flags, void *data);

/* Filesystem ops */
vfs2_filesystem_t systemfs_filesystem = {
    .flags = 0,
    .name  = "systemfs",
    .fs_mounts = NULL,
    .mount = systemfs_mount
};

/* File operations */
static vfs_file_ops_t systemfs_file_ops = {
    .open           = NULL,
    .close          = NULL,
    .read           = NULL,
    .write          = NULL,
    .ioctl          = NULL,
    .get_entries    = NULL,
    .poll           = NULL,
    .poll_events    = NULL,
    .mmap           = NULL,
    .mmap_prepare   = NULL,
    .munmap         = NULL,
};

/* Inode operations */
static vfs_inode_ops_t systemfs_inode_ops = {
    .create     = NULL,
    .destroy    = NULL,
    .getattr    = NULL,
    .link       = NULL,
    .lookup     = NULL,
    .mkdir      = NULL,
    .readlink   = NULL,
    .rmdir      = NULL,
    .setattr    = NULL,
    .symlink    = NULL,
    .truncate   = NULL,
    .unlink     = NULL,
};

/**
 * @brief Create SystemFS node
 */
static systemfs_node_t *systemfs_node() {
    systemfs_node_t *n = slab_allocate(systemfs_node_cache);
    if (!n) return NULL;

    memset(n, 0, sizeof(systemfs_node_t));
    MUTEX_INIT(&n->lck);

    n->attr.atime = n->attr.mtime = n->attr.ctime = VFS_NOW();
    n->attr.ino = vfs_getNextInode();
    n->attr.mode = 0755;
    n->attr.nlink = 1;
    n->attr.type = VFS_FILE;
    return n;
}

/**
 * @brief SystemFS mount
 */
int systemfs_mount(vfs2_filesystem_t *filesystem, vfs_mount_t *mount_dst, char *src, unsigned long flags, void *data) {
    vfs_inode_t *root_inode = vfs2_inode();
    if (!root_inode) return -ENOMEM;

    root_inode->ops = &systemfs_inode_ops;
    root_inode->f_ops = &systemfs_file_ops;
    memcpy(&root_inode->attr, &systemfs_root->attr, sizeof(vfs_inode_attr_t));
    root_inode->priv = systemfs_root;
    root_inode->mount = mount_dst;

    mount_dst->root = root_inode;
    mount_dst->ops = NULL;
    return 0;
}

/**
 * @brief systemfs init
 */
static int systemfs_init() {
    vfs_register(&systemfs_filesystem);
    assert((systemfs_node_cache = slab_createCache("systemfs node cache", SLAB_CACHE_DEFAULT, sizeof(systemfs_node_t), 0, NULL, NULL)));

    systemfs_root = systemfs_node();
    systemfs_root->name = "/";
    systemfs_root->ops = NULL;
    systemfs_root->children = hashmap_create("systemfs children", 5);
    systemfs_root->attr.nlink = 2;
    systemfs_root->attr.type = VFS_DIRECTORY;

    return 0;
}

FS_INIT_ROUTINE(systemfs, INIT_FLAG_DEFAULT, systemfs_init);