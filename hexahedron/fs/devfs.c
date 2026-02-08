/**
 * @file hexahedron/fs/devfs.c
 * @brief Device filesystem
 * 
 * Handles the creation, removal, and management of /device entries
 * init will mount devfs onto /device
 * 
 * It's kinda crummy to be honest, but eh, what are you gonna do.
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/fs/devfs.h>
#include <kernel/fs/vfs_new.h>
#include <kernel/debug.h>
#include <kernel/mm/vmm.h>
#include <kernel/init.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "FS:DEVFS", __VA_ARGS__)

static int devfs_open(vfs_file_t *file, unsigned long flags);
static int devfs_close(vfs_file_t *file);
static ssize_t devfs_read(vfs_file_t *file, loff_t off, size_t size, char *buffer);
static ssize_t devfs_write(vfs_file_t *file, loff_t off, size_t size, const char *buffer);
static int devfs_ioctl(vfs_file_t *file, long request, void *argp);
static int devfs_lseek(vfs_file_t *file, loff_t off, int whence, loff_t *pos);
static int devfs_get_entries(vfs_file_t *file, vfs_dir_context_t *ctx);
static int devfs_poll(vfs_file_t *file, poll_waiter_t *waiter, poll_events_t events);
static poll_events_t devfs_poll_events(vfs_file_t *file);
static int devfs_mmap(vfs_file_t *file, void *addr, size_t len, off_t off, mmu_flags_t flags);
static int devfs_mmap_prepare(vfs_file_t *file, vmm_memory_range_t *range);
static int devfs_munmap(vfs_file_t *file, void *addr, size_t size, off_t offset);

static int devfs_mount(vfs2_filesystem_t *filesystem, vfs_mount_t *mount_dst, char *src, unsigned long flags, void *data);
static int devfs_lookup(vfs_inode_t *inode, char *name, vfs_inode_t **inode_dst);
static int devfs_write_inode(vfs_inode_t *inode);
static int devfs_getattr(vfs_inode_t *inode, vfs_inode_attr_t *attr);
static int devfs_unimplemented();

/* File operations */
static vfs_file_ops_t devfs_file_ops = {
    .open           = devfs_open,
    .close          = devfs_close,
    .read           = devfs_read,
    .write          = devfs_write,
    .ioctl          = devfs_ioctl,
    .lseek          = devfs_lseek,
    .get_entries    = devfs_get_entries,
    .poll_events    = devfs_poll_events,
    .poll           = devfs_poll,
    .mmap           = devfs_mmap,
    .mmap_prepare   = devfs_mmap_prepare,
    .munmap         = devfs_munmap,
};

/* Inode operations */
static vfs_inode_ops_t devfs_inode_ops = {
    .create     = devfs_unimplemented,
    .destroy    = NULL,
    .link       = devfs_unimplemented,
    .lookup     = devfs_lookup,
    .mkdir      = devfs_unimplemented,
    .readlink   = NULL, // function prototype piss me off
    .rmdir      = devfs_unimplemented,
    .getattr    = devfs_getattr,
    .setattr    = devfs_unimplemented,
    .symlink    = devfs_unimplemented,
    .truncate   = devfs_unimplemented,
    .unlink     = devfs_unimplemented
};

/* Mount operations */
static vfs_mount_ops_t devfs_mount_ops = {
    .unmount = NULL,
    .write_inode = devfs_write_inode
};

/* Filesystem */
static vfs2_filesystem_t devfs = {
    .name = "devfs",
    .flags = 0,
    .mount = devfs_mount,
    .fs_mounts = NULL
};

/* Root node */
static devfs_node_t __devfs_root = {
    .name = "root",
    .ops = NULL,
    .parent = NULL,
    .ino = 1,
    .major = 0,
    .minor = 0,
    .priv = NULL,
    .children = NULL,
    .lck = MUTEX_INITIALIZER,
};

__attribute__((used)) devfs_node_t *devfs_root = &__devfs_root;

/**
 * @brief devfs unimplemented
 */
static int devfs_unimplemented() {
    LOG(ERR, "devfs_unimplemented\n");
    return -ENODEV;
}

/**
 * @brief devfs write inode
 */
static int devfs_write_inode(vfs_inode_t *inode) {
    devfs_node_t *n = (devfs_node_t*)inode->priv;
    memcpy(&n->attr, &inode->attr, sizeof(vfs_inode_attr_t));
    return 0;
}

/**
 * @brief devfs open
 */
static int devfs_open(vfs_file_t *file, unsigned long flags) {
    devfs_node_t *n = file->inode->priv;
    file->priv = n;

    if (n->ops && n->ops->open) {
        return n->ops->open(n, flags);
    }

    return 0;
}

/**
 * @brief devfs close
 */
static int devfs_close(vfs_file_t *file) {
    devfs_node_t *n = file->priv;
    if (n->ops && n->ops->close) return n->ops->close(n);
    return 0;
}

/**
 * @brief devfs read
 */
static ssize_t devfs_read(vfs_file_t *file, loff_t off, size_t size, char *buffer) {
    if (file->inode->attr.type == VFS_DIRECTORY) return -EISDIR;
    if (file->inode->attr.type != VFS_BLOCKDEVICE && file->inode->attr.type != VFS_CHARDEVICE) return -ENODEV;

    devfs_node_t *n = file->priv;
    if (n->ops && n->ops->read) {
        return n->ops->read(n, off, size, buffer);
    } else {
        return -ENODEV;
    }
}

/**
 * @brief devfs write
 */
static ssize_t devfs_write(vfs_file_t *file, loff_t off, size_t size, const char *buffer) {
    if (file->inode->attr.type == VFS_DIRECTORY) return -EISDIR;
    if (file->inode->attr.type != VFS_BLOCKDEVICE && file->inode->attr.type != VFS_CHARDEVICE) return -ENODEV;

    devfs_node_t *n = file->priv;
    if (n->ops && n->ops->write) {
        return n->ops->write(n, off, size, buffer);
    } else {
        return -ENODEV;
    }
}

/**
 * @brief devfs ioctl
 */
static int devfs_ioctl(vfs_file_t *file, long request, void *argp) {
    if (file->inode->attr.type == VFS_DIRECTORY) return -EISDIR;
    if (file->inode->attr.type != VFS_BLOCKDEVICE && file->inode->attr.type != VFS_CHARDEVICE) return -ENODEV;

    devfs_node_t *n = file->priv;
    if (n->ops && n->ops->ioctl) {
        return n->ops->ioctl(n, request, argp);
    } else {
        return -ENOTTY;
    }
}

/**
 * @brief devfs get entries
 */
static int devfs_get_entries(vfs_file_t *file, vfs_dir_context_t *ctx) {
    devfs_node_t *n = file->priv;
    if (n->attr.type != VFS_DIRECTORY) return -ENOTDIR;

    // read the entries from the directory
    if (ctx->dirpos == 0) {
        ctx->name = ".";
        ctx->ino = file->inode->attr.ino;
        ctx->type = DT_DIR;
        return 0;
    } else if (ctx->dirpos == 1) {
        ctx->name = "..";
        if (n->parent) ctx->ino = n->parent->ino;
        else ctx->ino = file->inode->attr.ino;
        ctx->type = DT_DIR;
        return 0;
    }

    int i = ctx->dirpos-2;
    int j = 0;

    mutex_acquire(&n->lck);
    list_t *c = hashmap_values(n->children);

    foreach(cn, c) {
        if (i == j) {
            devfs_node_t *ch = (devfs_node_t*)cn->value;
            ctx->name = ch->name;
            ctx->ino = ch->ino;
            // TODO d_type

            list_destroy(c, false);
            mutex_release(&n->lck);
            return 0;
        }

        j++;

    }

    list_destroy(c, false);
    mutex_release(&n->lck);
    return 1;
}

/**
 * @brief devfs lseek
 */
static int devfs_lseek(vfs_file_t *file, loff_t off, int whence, loff_t *pos) {
    devfs_node_t *n = file->priv;
    if (n->ops && n->ops->lseek) return n->ops->lseek(n, off, whence, pos);

    switch (whence) {
        // SEEK_END is not supported so we just set to the offset
        case SEEK_SET: case SEEK_END: *pos = off; return 0;
        case SEEK_CUR: *pos += off; return 0;
        default: assert(0);
    }
}

/**
 * @brief devfs poll events
 */
static poll_events_t devfs_poll_events(vfs_file_t *file) {
    if (file->inode->attr.type == VFS_DIRECTORY) return -EISDIR;
    if (file->inode->attr.type != VFS_BLOCKDEVICE && file->inode->attr.type != VFS_CHARDEVICE) return -ENODEV;

    devfs_node_t *n = file->priv;
    if (n->ops && n->ops->poll_events) {
        return n->ops->poll_events(n);
    } else {
        return 0;
    }
}

/**
 * @brief devfs poll
 */
static int devfs_poll(vfs_file_t *file, poll_waiter_t *waiter, poll_events_t events) {
    if (file->inode->attr.type == VFS_DIRECTORY) return -EISDIR;
    if (file->inode->attr.type != VFS_BLOCKDEVICE && file->inode->attr.type != VFS_CHARDEVICE) return -ENODEV;

    devfs_node_t *n = file->priv;
    if (n->ops && n->ops->poll) {
        return n->ops->poll(n, waiter, events);
    } else {
        return 0;
    }
}

/**
 * @brief devfs lookup
 */
static int devfs_lookup(vfs_inode_t *inode, char *name, vfs_inode_t **inode_dst) {
    if (inode->attr.type != VFS_DIRECTORY) return -ENOTDIR;
    devfs_node_t *n = (devfs_node_t*)inode->priv;

    mutex_acquire(&n->lck);
    devfs_node_t *child = hashmap_get(n->children, name);
    mutex_release(&n->lck);
    if (!child) return -ENOENT;

    vfs_inode_t *ret = vfs_iget(inode->mount, child->ino);
    if (!ret) return -ENOMEM;

    if (ret->state & INODE_STATE_NEW) {
        memcpy(&ret->attr, &child->attr, sizeof(vfs_inode_attr_t));
        ret->ops = &devfs_inode_ops;
        ret->f_ops = &devfs_file_ops;
        ret->mount = inode->mount;
        ret->priv = (void*)child;
        if (!n->ops->mmap) ret->flags |= INODE_FLAG_MMAP_UNSUPPORTED; // Kinda a hack
        inode_created(ret);
    }

    *inode_dst = ret;
    return 0;
}

/**
 * @brief devfs get attr
 */
static int devfs_getattr(vfs_inode_t *inode, vfs_inode_attr_t *attr) {
    devfs_node_t *n = (devfs_node_t*)inode->priv;
    memcpy(attr, &n->attr, sizeof(vfs_inode_attr_t));
    return 0;
}

/**
 * @brief devfs mmap
 */
static int devfs_mmap(vfs_file_t *file, void *addr, size_t len, off_t off, mmu_flags_t flags) {
    devfs_node_t *node = file->priv;
    if (node->attr.type != VFS_BLOCKDEVICE && node->attr.type != VFS_CHARDEVICE) return -ENODEV;
    
    if (node->ops->mmap) {
        return node->ops->mmap(node, addr, len, off, flags);
    } else {
        return -ENODEV;
    }
}

/**
 * @brief devfs mmap prepare
 */
static int devfs_mmap_prepare(vfs_file_t *file, vmm_memory_range_t *range) {
    devfs_node_t *node = file->priv;
    if (node->attr.type != VFS_BLOCKDEVICE && node->attr.type != VFS_CHARDEVICE) return -ENODEV;
    
    if (node->ops->mmap_prepare) {
        return node->ops->mmap_prepare(node, range);
    } else {
        return 0;
    }
}

/**
 * @brief devfs munmap
 */
static int devfs_munmap(vfs_file_t *file, void *addr, size_t size, off_t offset) {
    devfs_node_t *node = file->priv;
    if (node->attr.type != VFS_BLOCKDEVICE && node->attr.type != VFS_CHARDEVICE) return -ENODEV;
    
    if (node->ops->munmap) {
        return node->ops->munmap(node, addr, size, offset);
    } else {
        return -ENODEV;
    }
}

/**
 * @brief Create device filesystem node
 */
static devfs_node_t *devfs_node() {
    devfs_node_t *n = kmalloc(sizeof(devfs_node_t));
    n->children = NULL;
    n->ino = vfs_getNextInode();
    MUTEX_INIT(&n->lck);

    // setup node attributes
    n->attr.atime = n->attr.mtime = n->attr.ctime = VFS_NOW();
    n->attr.nlink = 1;
    n->attr.size = 0;
    n->attr.type = VFS_BLOCKDEVICE;
    n->attr.mode = 0755;
    n->attr.ino = n->ino;

    return n;
}

/**
 * @brief Create device filesystem directory
 * @param parent The parent to create the directory under
 * @param name The name of the directory
 */
devfs_node_t *devfs_createDirectory(devfs_node_t *parent, char *name) {
    assert(parent->children);
    devfs_node_t *n = devfs_node();
    n->name = strdup(name);
    n->ops = NULL;
    n->parent = parent;
    n->attr.nlink = 2;
    n->attr.type = VFS_DIRECTORY;
    n->children = hashmap_create("devfs node children", 5);
    
    mutex_acquire(&parent->lck);
    hashmap_set(parent->children, name, n);
    mutex_release(&parent->lck);
    return n;
}

/**
 * @brief Register a new device into the device filesystem
 * @param parent The parent to register under (can be any node, devfs_root probably best)
 * @param name The name of the device to register
 * @param type The type of the device (VFS_DIRECTORY, VFS_FILE, VFS_BLOCKDEVICE, ...)
 * @param ops The operations of the new device being registered
 * @param major The major device number (DEVFS_MAJOR_...)
 * @param minor The minor device number (your choosing)
 * @param priv Private
 * @returns New device node
 */
devfs_node_t *devfs_register(devfs_node_t *parent, char *name, int type, devfs_ops_t *ops, dev_t major, dev_t minor, void *priv) {
    devfs_node_t *node = devfs_node();
    node->name = strdup(name);
    node->major = major;
    node->minor = minor;
    node->ops = ops;
    node->priv = priv;
    node->attr.type = type;

    assert(type == VFS_BLOCKDEVICE || type == VFS_CHARDEVICE);

    char name_copy[strlen(name) + 1]; // !!!
    strcpy(name_copy, name);

    char *save;
    char *pch = strtok_r(name_copy, "/", &save);

    while (pch) {
        char *nxt = strtok_r(NULL, "/", &save);
        if (nxt) {
            assert(strcmp(pch, "..") && strcmp(pch, "."));

            mutex_acquire(&parent->lck);
            devfs_node_t *nn = hashmap_get(parent->children, pch);
            mutex_release(&parent->lck);

            parent = nn;
            if (!nn) {
                LOG(ERR, "Failed to register %s as %s doesn't exist\n", name, pch);
                return NULL;
            }
        } else {
            break;
        }

        pch = nxt;
    }


    assert(parent->children);
    node->parent = parent;
    mutex_acquire(&parent->lck);
    hashmap_set(parent->children, pch, node);
    mutex_release(&parent->lck);
    return node;
}

/**
 * @brief Unregister a device node
 * @param dev The device node to unregister
 */
int devfs_unregister(devfs_node_t *node) {
    // recursive
    if (node->children) {
        // TODO: dont care enough rn
        assert(0 && "devfs_unregister() for directories is not implemented");
    }

    if (node->parent) {
        mutex_acquire(&node->parent->lck);
        hashmap_remove(node->parent->children, node->name);      
        mutex_release(&node->parent->lck);  
    }

    // TODO: memory leak in node->name
    kfree(node);
    return 0;
}

/**
 * @brief Get a device node by name
 * @param name The name of the device node to get
 * @returns Node or NULL
 */
devfs_node_t *devfs_get(char *name) {
    devfs_node_t *cur = devfs_root;
    if (!strcmp(name, "/") || !(*name)) {
        return cur;
    }

    char tmp[256];
    strncpy(tmp, name, 256);

    char *save;
    char *pch = strtok_r(tmp, "/", &save);
    while (pch) {
        mutex_acquire(&cur->lck);
        devfs_node_t *n = hashmap_get(cur->children, pch);
        mutex_release(&cur->lck);
        cur = n;
        if (!cur) return NULL;
        pch = strtok_r(NULL, "/", &save);
    }

    return cur;
}

/**
 * @brief Mount devfs
 */
static int devfs_mount(vfs2_filesystem_t *filesystem, vfs_mount_t *mount_dst, char *src, unsigned long flags, void *data) {
    vfs_inode_t *root_inode = vfs2_inode();
    if (!root_inode) return -ENOMEM;

    root_inode->ops = &devfs_inode_ops;
    root_inode->f_ops = &devfs_file_ops;
    memcpy(&root_inode->attr, &devfs_root->attr, sizeof(vfs_inode_attr_t));
    root_inode->priv = (void*)devfs_root;
    root_inode->mount = mount_dst;
    
    mount_dst->root = root_inode;
    mount_dst->ops = &devfs_mount_ops;
    return 0;
}

/**
 * @brief Initialize devfs
 */
static int devfs_init() {
    // setup devfs root
    devfs_root->children = hashmap_create("devfs node children", 5);
    devfs_root->attr.atime = devfs_root->attr.mtime = devfs_root->attr.ctime = VFS_NOW();
    devfs_root->attr.nlink = 1;
    devfs_root->attr.size = 0;
    devfs_root->attr.type = VFS_DIRECTORY;
    devfs_root->attr.mode = 0775;
    devfs_root->attr.ino = 1;

    vfs_register(&devfs);
    return 0;
}

FS_INIT_ROUTINE(devfs, INIT_FLAG_DEFAULT, devfs_init);