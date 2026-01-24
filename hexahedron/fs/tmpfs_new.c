/**
 * @file hexahedron/fs/tempfs.c
 * @brief TempFS for temporary in-memory filesystems
 * 
 * @todo The existing @c tmpfs_write and @c tmpfs_read will be removed when the page cache is done
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/fs/tmpfs_new.h>
#include <kernel/debug.h>
#include <kernel/mm/vmm.h>
#include <kernel/init.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "FS:TMPFS", __VA_ARGS__)

/* Protos */
static int tmpfs_open(vfs_file_t *file, unsigned long flags);
static ssize_t tmpfs_read(vfs_file_t *file, loff_t off, ssize_t size, char *buffer);
static ssize_t tmpfs_write(vfs_file_t *file, loff_t off, ssize_t size, const char *buffer);
static int tmpfs_get_entries(vfs_file_t *file, vfs_dir_context_t *ctx);
static int tmpfs_create(vfs_inode_t *parent, char *name, mode_t mode, vfs_inode_t **ino_output);
static int tmpfs_mkdir(vfs_inode_t *parent, char *name, mode_t mode, vfs_inode_t **ino_output);
static int tmpfs_lookup(vfs_inode_t *inode, char *name, vfs_inode_t **output);
static int tmpfs_mount(vfs2_filesystem_t *filesystem, vfs_mount_t *mount, char *src, unsigned long flags, void *data);
static int tmpfs_truncate(vfs_inode_t *inode, size_t size);
static int tmpfs_symlink(vfs_inode_t *parent, char *link_contents, char *link_name, vfs_inode_t **ino_output);
static ssize_t tmpfs_readlink(vfs_inode_t *inode, char *buffer, size_t maxlen);


/* Filesystem */
static vfs2_filesystem_t tmpfs_filesystem = {
    .name = "tmpfs",
    .mount = tmpfs_mount,
};

/* Inode ops */
static vfs_inode_ops_t tmpfs_inode_ops = {
    .create = tmpfs_create,
    .destroy = NULL,
    .getattr = NULL,
    .link = NULL,
    .lookup = tmpfs_lookup,
    .mkdir = tmpfs_mkdir,
    .readlink = tmpfs_readlink,
    .rmdir = NULL,
    .setattr = NULL,
    .symlink = tmpfs_symlink,
    .truncate = tmpfs_truncate,
    .unlink = NULL
};

/* File ops */
static vfs_file_ops_t tmpfs_file_ops = {
    .open = tmpfs_open,
    .close = NULL,
    .read = tmpfs_read,
    .write = tmpfs_write,
    .get_entries = tmpfs_get_entries,
    .ioctl = NULL
};

/* Cache */
slab_cache_t *tmpfs_node_cache = NULL;


/**
 * @brief tmpfs open
 */
static int tmpfs_open(vfs_file_t *file, unsigned long flags) {
    file->priv = file->inode->priv;
    return 0;
}

/**
 * @brief tmpfs get entries
 */
static int tmpfs_get_entries(vfs_file_t *file, vfs_dir_context_t *ctx) {
    tmpfs_node_t *node = (tmpfs_node_t*)file->priv;
    if (ctx->dirpos == 0) {
        ctx->name = ".";
        ctx->ino = file->inode->attr.ino;
        ctx->type = DT_DIR;
        return 0;
    } else if (ctx->dirpos == 1) {
        ctx->name = "..";
        if (node->parent) ctx->ino = node->parent->ino;
        else ctx->ino = file->inode->attr.ino;
        ctx->type = DT_DIR;
        return 0;
    }

    int i = ctx->dirpos - 2;
    int j = 0;
    mutex_acquire(&node->lck);
    list_t *keys = hashmap_keys(node->dir.children);
    
    foreach(kn, keys) {
        tmpfs_node_t *n_child = (tmpfs_node_t*)hashmap_get(node->dir.children, kn->value);
        assert(n_child);
        if (i == j) {
            mutex_release(&node->lck);
            ctx->name = kn->value;
            ctx->ino = n_child->ino;
            ctx->type = 0; // TODO
            list_destroy(keys, false);
            return 0;
        }  

        j++;
    }

    mutex_release(&node->lck);
    list_destroy(keys, false);
    
    return 1;
}

/**
 * @brief tmpfs create
 */
static int tmpfs_create(vfs_inode_t *parent, char *name, mode_t mode, vfs_inode_t **ino_output) {
    tmpfs_node_t *node = (tmpfs_node_t*)parent->priv;    
    mutex_acquire(&node->lck);
    if (hashmap_get(node->dir.children, name)) { mutex_release(&node->lck); return -EEXIST; }

    tmpfs_node_t *new_node = slab_allocate(tmpfs_node_cache);
    if (!new_node) { mutex_release(&node->lck); return -ENOMEM; }

    MUTEX_INIT(&new_node->lck);
    new_node->file.page_list = NULL;
    new_node->file.page_count = 0;
    new_node->parent = node;
    
    new_node->attr.type = VFS_FILE;
    new_node->attr.ino = vfs_getNextInode();
    new_node->attr.atime = new_node->attr.mtime = new_node->attr.ctime = VFS_NOW();
    new_node->attr.mode = mode;
    new_node->attr.nlink = 1;

    // Create an inode
    vfs_inode_t *ino = vfs2_inode();
    if (!ino) { slab_free(tmpfs_node_cache, new_node); mutex_release(&node->lck); return -ENOMEM; }
    memcpy(&ino->attr, &new_node->attr, sizeof(vfs_inode_attr_t));
    ino->mount = parent->mount;
    ino->ops = &tmpfs_inode_ops;
    ino->f_ops = &tmpfs_file_ops;
    ino->priv = new_node;
    new_node->ino = ino->attr.ino;

    hashmap_set(node->dir.children, name, new_node);
    *ino_output = ino;
    mutex_release(&node->lck);
    return 0;
}

/**
 * @brief tmpfs symlink
 */
static int tmpfs_symlink(vfs_inode_t *parent, char *link_contents, char *link_name, vfs_inode_t **ino_output) {
    tmpfs_node_t *n = parent->priv;

    // test if exists
    mutex_acquire(&n->lck);
    if (hashmap_has(n->dir.children, link_name)) {
        mutex_release(&n->lck);
        return -EEXIST;
    }
    mutex_release(&n->lck);

    tmpfs_node_t *new_node = slab_allocate(tmpfs_node_cache);
    if (!new_node) { return -ENOMEM; }

    MUTEX_INIT(&new_node->lck);
    new_node->file.page_list = NULL;
    new_node->file.page_count = 0;
    new_node->parent = n;
    
    new_node->attr.type = VFS_SYMLINK;
    new_node->attr.ino = vfs_getNextInode();
    new_node->attr.atime = new_node->attr.mtime = new_node->attr.ctime = VFS_NOW();
    new_node->attr.mode = 0755; // TODO
    new_node->attr.nlink = 1;

    new_node->symlink.path = strdup(link_contents);

        // Create an inode
    vfs_inode_t *ino = vfs2_inode();
    if (!ino) { slab_free(tmpfs_node_cache, new_node);  return -ENOMEM; }
    memcpy(&ino->attr, &new_node->attr, sizeof(vfs_inode_attr_t));
    ino->mount = parent->mount;
    ino->ops = &tmpfs_inode_ops;
    ino->f_ops = &tmpfs_file_ops;
    ino->priv = new_node;
    new_node->ino = ino->attr.ino;
    
    mutex_acquire(&n->lck);
    hashmap_set(n->dir.children, link_name, new_node);
    mutex_release(&n->lck);

    *ino_output = ino;
    return 0;
}

/**
 * @brief tmpfs readlink
 */
static ssize_t tmpfs_readlink(vfs_inode_t *inode, char *buffer, size_t maxlen) {
    if (inode->attr.type != VFS_SYMLINK) return -EINVAL;
    tmpfs_node_t *n = inode->priv;

    size_t len = strlen(n->symlink.path);
    if (maxlen > len) maxlen = len;

    strncpy(buffer, n->symlink.path, maxlen);
    return maxlen;
}

/**
 * @brief tmpfs mkdir
 */
static int tmpfs_mkdir(vfs_inode_t *parent, char *name, mode_t mode, vfs_inode_t **ino_output) {
    tmpfs_node_t *node = parent->priv;
    mutex_acquire(&node->lck);

    // Check if existing
    if (hashmap_has(node->dir.children, name)) {
        mutex_release(&node->lck);
        return -EEXIST;
    }

    // If not then create the new node
    tmpfs_node_t *new = slab_allocate(tmpfs_node_cache);
    if (!new) { mutex_release(&node->lck); return -ENOMEM; }

    MUTEX_INIT(&new->lck);
    new->dir.children = hashmap_create("tmpfs dir children", 10);
    new->parent = node;
    
    new->attr.type = VFS_DIRECTORY;
    new->attr.nlink = 2; // . and ..
    new->attr.mode = mode;
    new->attr.ino = vfs_getNextInode();

    // Now make the inode
    vfs_inode_t *i = vfs2_inode();
    if (!i) { slab_free(tmpfs_node_cache, new); mutex_release(&node->lck); return -ENOMEM; }
    memcpy(&i->attr, &new->attr, sizeof(vfs_inode_attr_t));
    i->mount = parent->mount;
    i->ops = &tmpfs_inode_ops;
    i->f_ops = &tmpfs_file_ops;
    i->priv = (void*)new;
    
    new->ino = i->attr.ino;

    hashmap_set(node->dir.children, name, new);
    if (ino_output) *ino_output = i;
    mutex_release(&node->lck);

    return 0;
}

/**
 * @brief tmpfs lookup
 */
static int tmpfs_lookup(vfs_inode_t *inode, char *name, vfs_inode_t **output) {
    tmpfs_node_t *node = inode->priv;
    mutex_acquire(&node->lck);

    tmpfs_node_t *c = hashmap_get(node->dir.children, name);
    if (!c) {
        mutex_release(&node->lck);
        return -ENOENT;
    }

    // Lookup the inode's number in the VFS cache
    vfs_inode_t *i = vfs_iget(inode->mount, c->ino);
    if (i->state & INODE_STATE_NEW) {
        // Configure the inode
        memcpy(&i->attr, &c->attr, sizeof(vfs_inode_attr_t));
        i->priv = (void*)c;
        i->mount = inode->mount;
        i->ops = &tmpfs_inode_ops;
        i->f_ops = &tmpfs_file_ops;
        inode_created(i);
    }

    *output = i;

    mutex_release(&node->lck);
    return 0;
}

/**
 * @brief tmpfs truncate
 */
static int tmpfs_truncate(vfs_inode_t *inode, size_t size) {
    // truncate
    size_t in_pages = PAGE_ALIGN_UP(size) / PAGE_SIZE;
    
    LOG(DEBUG, "tmpfs_truncate size %d in_pages %d\n", size, in_pages);

    tmpfs_node_t *n = inode->priv;
    mutex_acquire(&n->lck);
    if (in_pages < n->file.page_count) {
        // We are shrinking down the file
        // Go through and free any existing pages
        for (unsigned i = n->file.page_count; i > in_pages; i++) {
            pmm_freePage(n->file.page_list[i]);
        }

        n->file.page_list = krealloc(n->file.page_list, in_pages * sizeof(uintptr_t*));
    } else if (in_pages > n->file.page_count) {
        // We are growing the file
        n->file.page_list = krealloc(n->file.page_list, in_pages * sizeof(uintptr_t*));
        for (unsigned i = n->file.page_count; i < in_pages; i++) {
            n->file.page_list[i] = pmm_allocatePage(ZONE_DEFAULT);
        }
    }

    n->file.page_count = in_pages;
    n->attr.size = size;
    inode->attr.size = size;
    mutex_release(&n->lck);

    return 0;
}

/**
 * @brief tmpfs read
 */
static ssize_t tmpfs_read(vfs_file_t *file, loff_t off, ssize_t size, char *buffer) {
    tmpfs_node_t *node = file->priv;

    if (node->attr.type == VFS_DIRECTORY) return -EISDIR;
    if (node->attr.type != VFS_FILE) return -EINVAL;

    if (off >= file->inode->attr.size) {
        return 0;
    }

    if (off + size > file->inode->attr.size) {
        size = file->inode->attr.size - off;
    }

    mutex_acquire(&node->lck); // TODO: rwlock/rwsem

    // Well, now we can just read from the pages.
    uintptr_t *pgl = node->file.page_list;
    size_t remaining = size;
    size_t pg_ind = off / PAGE_SIZE;
    size_t pg_off = off % PAGE_SIZE;
    size_t bpos = 0;

    while (remaining) {
        size_t chunk = PAGE_SIZE - pg_off;
        if (chunk > remaining) chunk = remaining;

        // map the corresponding page into memory
        uintptr_t pg_tmp = arch_mmu_remap_physical(pgl[pg_ind], PAGE_SIZE, REMAP_TEMPORARY);

        memcpy(buffer + bpos, (void*)pg_tmp + pg_off, chunk);

    #ifndef __ARCH_X86_64__
        arch_mmu_unmap_physical(pg_tmp, PAGE_SIZE);
    #endif

        bpos += chunk;
        remaining -= chunk;
        pg_ind++;
        pg_off = 0;
    }

    mutex_release(&node->lck);
    return size;
}

/**
 * @brief tmpfs write
 */
static ssize_t tmpfs_write(vfs_file_t *file, loff_t off, ssize_t size, const char *buffer) {
    tmpfs_node_t *n = file->priv;
    if (n->attr.type == VFS_DIRECTORY) return -EISDIR;
    if (n->attr.type != VFS_FILE) return -EINVAL;

    if (off + size >= file->inode->attr.size) {
        int r = tmpfs_truncate(file->inode, off+size);
        if (r) return r;
    }

    mutex_acquire(&n->lck);
    
     // Well, now we can just write to the pages.
    uintptr_t *pgl = n->file.page_list;
    size_t remaining = size;
    size_t pg_ind = off / PAGE_SIZE;
    size_t pg_off = off % PAGE_SIZE;
    size_t bpos = 0;

    while (remaining) {
        size_t chunk = PAGE_SIZE - pg_off;
        if (chunk > remaining) chunk = remaining;

        // map the corresponding page into memory
        uintptr_t pg_tmp = arch_mmu_remap_physical(pgl[pg_ind], PAGE_SIZE, REMAP_TEMPORARY);

        memcpy((void*)pg_tmp + pg_off, buffer + bpos, chunk);

    #ifndef __ARCH_X86_64__
        arch_mmu_unmap_physical(pg_tmp, PAGE_SIZE);
    #endif

        bpos += chunk;
        remaining -= chunk;
        pg_ind++;
        pg_off = 0;
    }
    
    mutex_release(&n->lck);

    return size;
}

/**
 * @brief Mount to tmpfs
 */
static int tmpfs_mount(vfs2_filesystem_t *filesystem, vfs_mount_t *mount_dst, char *src, unsigned long flags, void *data) {
    // Create the root node
    tmpfs_node_t *node = slab_allocate(tmpfs_node_cache);
    if (!node) return -ENOMEM;
    MUTEX_INIT(&node->lck);
    node->dir.children = hashmap_create("tmpfs node children", 10);
    node->parent = NULL;

    node->attr.type = VFS_DIRECTORY;
    node->attr.atime = node->attr.mtime = node->attr.ctime =  VFS_NOW();
    node->attr.mode = 0755;
    node->attr.ino = vfs_getNextInode();
    node->attr.nlink = 2;
    
    // Now create the root inode
    vfs_inode_t *root_inode = vfs2_inode();
    if (!root_inode) { slab_free(tmpfs_node_cache, node); return -ENOMEM; }
    root_inode->ops = &tmpfs_inode_ops;
    root_inode->f_ops = &tmpfs_file_ops;
    memcpy(&root_inode->attr, &node->attr, sizeof(vfs_inode_attr_t));
    root_inode->mount = mount_dst;
    root_inode->priv = (void*)node;

    mount_dst->root = root_inode;
    
    return 0;
}

/**
 * @brief Initialize tmpfs
 */
static int tmpfs_init() {
    tmpfs_node_cache = slab_createCache("tmpfs node cache", SLAB_CACHE_DEFAULT, sizeof(tmpfs_node_t), 0, NULL, NULL);
    vfs_register(&tmpfs_filesystem);
    return 0;
}

FS_INIT_ROUTINE(tmpfs_new, INIT_FLAG_DEFAULT, tmpfs_init);