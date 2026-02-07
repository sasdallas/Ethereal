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
#include <stdio.h>

/* Cache */
static slab_cache_t *systemfs_node_cache;

/* Root */
systemfs_node_t *systemfs_root;

/* Protos */
int systemfs_mount(vfs2_filesystem_t *filesystem, vfs_mount_t *mount_dst, char *src, unsigned long flags, void *data);

/* Log method */
#define LOG(status, ...) dprintf_module(status, "FS:SYSTEMFS", __VA_ARGS__)

/* Filesystem ops */
vfs2_filesystem_t systemfs_filesystem = {
    .flags = 0,
    .name  = "systemfs",
    .fs_mounts = NULL,
    .mount = systemfs_mount
};

/* File operations */
static int systemfs_open(vfs_file_t *file, unsigned long flags);
static int systemfs_close(vfs_file_t *file);
static ssize_t systemfs_read(vfs_file_t *node, loff_t off, size_t size, char *buffer);
static ssize_t systemfs_write(vfs_file_t *file, loff_t off, size_t size, const char *buffer);
static int systemfs_ioctl(vfs_file_t *file, long request, void *argp);
static int systemfs_get_entries(vfs_file_t *file, vfs_dir_context_t *ctx);

static vfs_file_ops_t systemfs_file_ops = {
    .open           = systemfs_open,
    .close          = systemfs_close,
    .read           = systemfs_read,
    .write          = systemfs_write,
    .ioctl          = systemfs_ioctl,
    .get_entries    = systemfs_get_entries,
    .poll           = NULL,
    .poll_events    = NULL,
    .mmap           = NULL,
    .mmap_prepare   = NULL,
    .munmap         = NULL,
};

/* Inode operations */
static int systemfs_getattr(vfs_inode_t *inode, vfs_inode_attr_t *attr);
static int systemfs_lookup(vfs_inode_t *inode, char *name, vfs_inode_t **ino_output);
static vfs_inode_ops_t systemfs_inode_ops = {
    .create     = NULL,
    .destroy    = NULL,
    .getattr    = systemfs_getattr,
    .link       = NULL,
    .lookup     = systemfs_lookup,
    .mkdir      = NULL,
    .readlink   = NULL,
    .rmdir      = NULL,
    .setattr    = NULL,
    .symlink    = NULL,
    .truncate   = NULL,
    .unlink     = NULL,
};


/**
 * @brief systemfs open
 */
static int systemfs_open(vfs_file_t *file, unsigned long flags) {
    file->priv = file->inode->priv;
    systemfs_node_t *n = file->priv;
    if (n->ops && n->ops->open) return n->ops->open(n, flags);
    return 0;
}

/**
 * @brief systemfs close
 */
static int systemfs_close(vfs_file_t *file) {
    systemfs_node_t *node = file->priv;
    if (node->ops && node->ops->close) {
        return node->ops->close(node);
    }
    return 0;
}

/**
 * @brief systemfs read
 */
static ssize_t systemfs_read(vfs_file_t *node, loff_t off, size_t size, char *buffer) {
    systemfs_node_t *n = node->priv;
    if (n->attr.type == VFS_DIRECTORY) return -EISDIR;
    
    if (n->ops && n->ops->read) {
        return n->ops->read(n, off, size, buffer);
    }

    return -ENODEV;
}

/**
 * @brief systemfs write
 */
static ssize_t systemfs_write(vfs_file_t *file, loff_t off, size_t size, const char *buffer) {
    systemfs_node_t *n = file->priv;
    if (n->attr.type == VFS_DIRECTORY) return -EISDIR;
    
    if (n->ops && n->ops->write) {
        return n->ops->write(n, off, size, buffer);
    }

    return -ENODEV;
}

/**
 * @brief systemfs ioctl
 */
static int systemfs_ioctl(vfs_file_t *file, long request, void *argp) {
    systemfs_node_t *n = file->priv;
    if (n->attr.type == VFS_DIRECTORY) return -EISDIR;
    
    if (n->ops && n->ops->ioctl) {
        return n->ops->ioctl(n, request, argp);
    }

    return -ENODEV;
}

/**
 * @brief systemfs get entries
 */
static int systemfs_get_entries(vfs_file_t *file, vfs_dir_context_t *ctx) {
    systemfs_node_t *n = file->priv;
    if (n->attr.type != VFS_DIRECTORY) return -ENOTDIR;

    // read the entries from the directory
    if (ctx->dirpos == 0) {
        ctx->name = ".";
        ctx->ino = file->inode->attr.ino;
        ctx->type = DT_DIR;
        return 0;
    } else if (ctx->dirpos == 1) {
        ctx->name = "..";
        if (n->parent) ctx->ino = n->parent->attr.ino;
        else ctx->ino = file->inode->attr.ino;
        ctx->type = DT_DIR;
        return 0;
    }

    int i = ctx->dirpos-2;
    int j = 0;

    mutex_acquire(&n->lck);

    if (n->ops && n->ops->read_entry) {
        mutex_release(&n->lck);
        return n->ops->read_entry(n, ctx);
    } else {
        list_t *hk = hashmap_values(n->children);

        foreach (node, hk) {
            if (i == j) {
                systemfs_node_t *ch = node->value;
                ctx->name = ch->name;
                ctx->ino = ch->attr.ino;
                // TODO d_type
                list_destroy(hk, false);
                mutex_release(&n->lck);
                return 0;
            }

            j++;
        }

        list_destroy(hk, false);
    }

    mutex_release(&n->lck);
    return 1;
}

/**
 * @brief systemfs getattr
 */
static int systemfs_getattr(vfs_inode_t *inode, vfs_inode_attr_t *attr) {
    systemfs_node_t *n = inode->priv;
    memcpy(attr, &n->attr, sizeof(vfs_inode_attr_t));
    return 0;
}

/**
 * @brief systemfs lookup
 */
static int systemfs_lookup(vfs_inode_t *inode, char *name, vfs_inode_t **ino_output) {
    if (inode->attr.type != VFS_DIRECTORY) return -ENOTDIR;
    systemfs_node_t *n = (systemfs_node_t*)inode->priv;
    
    mutex_acquire(&n->lck);
    systemfs_node_t *child;
    if (n->ops && n->ops->lookup) {
        int r = n->ops->lookup(n, name, &child);
        if (r) { mutex_release(&n->lck); return r; }
    } else {
        child = hashmap_get(n->children, name);
    }

    mutex_release(&n->lck);
    if (!child) return -ENOENT;

    vfs_inode_t *ret = vfs_iget(inode->mount, child->attr.ino);
    if (!ret) return -ENOMEM;

    if (ret->state & INODE_STATE_NEW) {
        memcpy(&ret->attr, &child->attr, sizeof(vfs_inode_attr_t));
        ret->ops = &systemfs_inode_ops;
        ret->f_ops = &systemfs_file_ops;
        ret->mount = inode->mount;
        ret->priv = (void*)child;
        inode_created(ret);
    }

    *ino_output = ret;
    return 0;
}

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
 * @brief Register a new SystemFS node
 * @param parent The parent of the SystemFS node
 * @param name The name of the SystemFS node
 * @param type The type of the SystemFS node (VFS_xxx)
 * @param ops The operations of the SystemFS node
 * @param priv Private variables for the SystemFS node
 * @returns New node
 */
systemfs_node_t *systemfs_register(systemfs_node_t *parent, char *name, int type, systemfs_ops_t *ops, void *priv) {
    systemfs_node_t *node = systemfs_node();
    node->name = strdup(name);
    node->ops = ops;
    node->priv = priv;
    node->attr.type = type;
    
    if (type == VFS_DIRECTORY) {
        // maybe not here?
        node->children = hashmap_create("systemfs node children", 5);
    }

    char name_copy[strlen(name) + 1]; // !!!
    strcpy(name_copy, name);

    char *save;
    char *pch = strtok_r(name_copy, "/", &save);

    while (pch) {
        char *nxt = strtok_r(NULL, "/", &save);
        if (nxt) {
            assert(strcmp(pch, "..") && strcmp(pch, "."));

            mutex_acquire(&parent->lck);
            systemfs_node_t *nn = hashmap_get(parent->children, pch);
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
 * @brief Unregister SystemFS entry
 * @param parent The parent of the SystemFS entry to unregister
 * @param name The name of the SystemFS entry to unregister
 */
int systemfs_unregister(systemfs_node_t *parent, char *name) {
    assert(0 && "TBD");
    return 1;
}

/**
 * @brief Get SystemFS node
 * @param parent The parent of the SystemFS node
 * @param name The name of the SystemFS node
 */
systemfs_node_t *systemfs_get(systemfs_node_t *parent, char *name) {
    systemfs_node_t *cur = parent ? parent : systemfs_root;
    if (!strcmp(name, "/") || !(*name)) {
        return cur;
    }

    char tmp[256];
    strncpy(tmp, name, 256);

    char *save;
    char *pch = strtok_r(tmp, "/", &save);
    while (pch) {
        mutex_acquire(&cur->lck);
        systemfs_node_t *n = hashmap_get(cur->children, pch);
        mutex_release(&cur->lck);
        cur = n;
        if (!cur) return NULL;
        pch = strtok_r(NULL, "/", &save);
    }

    return cur;
}

/**
 * @brief Register a "simple" SystemFS node
 * 
 * Simple SystemFS nodes are nodes that only provide either reading/writing.
 * 
 * @param parent The parent of the SystemFS node
 * @param name The name of the SystemFS node
 * @param read Reading function for the SystemFS node
 * @param write Writing function for the SystemFS node
 * @param priv Private variable
 * @returns The node registered
 */
systemfs_node_t *systemfs_registerSimple(systemfs_node_t *parent, char *name, ssize_t (*read)(systemfs_node_t *, loff_t, size_t, char*), ssize_t (*write)(systemfs_node_t *, loff_t, size_t, const char*), void *priv) {
    // !!!: I don't like this :( Breaks ABI and is slow (LEAKS MEMORY IF UNREGISTERED)
    systemfs_ops_t *o = kmalloc(sizeof(systemfs_ops_t));
    o->read = read;
    o->write = write;
    o->open = NULL;
    o->close = NULL;
    o->ioctl = NULL;
    o->read_entry = NULL;
    o->lookup = NULL;

    return systemfs_register(parent, name, VFS_FILE, o, priv);
}

/**
 * @brief Create a link in SystemFS
 * @param parent The parent of the link
 * @param link_name The link name
 * @param link_contents The link contents
 */
systemfs_node_t *systemfs_symlink(systemfs_node_t *parent, char *link_name, char *link_contents) {
    assert(0 && "UNIMPLEMENTED");
}

/**
 * @brief Create and return a SystemFS directory
 * @param parent The parent of the directory
 * @param name The name of the directory
 * @note You can also do this with @c systemfs_register by specifying VFS_DIRECTORY as the type.
 */
systemfs_node_t *systemfs_createDirectory(systemfs_node_t *parent, char *name) {
    return systemfs_register(parent, name, VFS_DIRECTORY, NULL, NULL);
}

/**
 * @brief xvasprintf callback
 */
static int __systemfs_xvasprintf(void *user, char c) {
    struct systemfs_print_ctx *ctx = user;
    if (ctx->idx >= ctx->off && (size_t)(ctx->idx - ctx->off) < ctx->size) {
        ctx->buffer[ctx->idx-ctx->off] = c;
    }

    ctx->idx++;
    return 0;
}

/**
 * @brief Printf-formatter for SystemFS
 * @param buffer User-provided buffer
 * @param off Offset requested
 * @param size Size requested to read
 * @param fmt Format
 */
ssize_t systemfs_printf(char *buffer, loff_t off, size_t size, char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    struct systemfs_print_ctx ctx = {
        .buffer = buffer,
        .off = off,
        .size = size,
        .idx = 0,
    };

    xvasprintf(__systemfs_xvasprintf, &ctx, fmt, ap);
    va_end(ap);
    
    if (ctx.idx <= ctx.off) return 0;
    return (ctx.idx - ctx.off);
}

/**
 * @brief Append printf. Requires an initialized systemfs context by @c SYSTEMFS_PRINT_CTX_INIT()
 * @param ctx The context to print to
 * @param fmt The format
 */
ssize_t systemfs_printfAppend(struct systemfs_print_ctx *ctx, char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    xvasprintf(__systemfs_xvasprintf, ctx, fmt, ap);
    va_end(ap);
    if (ctx->idx <= ctx->off) return 0;
    return (ctx->idx - ctx->off);
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