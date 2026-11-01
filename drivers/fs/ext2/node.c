/**
 * @file drivers/fs/ext2/node.c
 * @brief Node manager
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
#include <kernel/mm/arch_mmu.h>
#include <kernel/mm/vmm.h>
#include <kernel/task/process.h>
#include <kernel/processor_data.h>
#include <kernel/panic.h>
#include <kernel/debug.h>
#include <string.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "DRIVER:EXT2:NODE", __VA_ARGS__)

/* Node cache */
slab_cache_t *node_cache = NULL;

/* Operations */
static int ext2_open(vfs_file_t *file, unsigned long flags);
static int ext2_get_entries(vfs_file_t *file, vfs_dir_context_t *ctx);
static int ext2_fsync(vfs_file_t *file);
vfs_file_ops_t ext2_file_ops = {
    .open = ext2_open,
    .close = NULL,
    .read = NULL,
    .write = NULL,
    .get_entries = ext2_get_entries,
    .ioctl = NULL,
    .mmap = NULL,
    .munmap = NULL,
    .mmap_prepare = NULL,
    .lseek = NULL,
    .poll = NULL,
    .poll_events = NULL,
    .check_flags = NULL,
    .fsync = ext2_fsync,
};

static int ext2_lookup(vfs_inode_t *inode, char *name, vfs_inode_t **out);
static int ext2_create(vfs_inode_t *parent, char *name, mode_t mode, vfs_inode_t **output);
static int ext2_mkdir(vfs_inode_t *parent, char *name, mode_t mode, vfs_inode_t **output);
static int ext2_truncate(vfs_inode_t *parent, size_t size);

static int ext2_unlink(vfs_inode_t *inode, vfs_inode_t *child, char *name);
static int ext2_rmdir(vfs_inode_t *inode, vfs_inode_t *child, char *name);
static int ext2_link(vfs_inode_t *inode, vfs_inode_t *parent, char *link_name);
static int ext2_symlink(vfs_inode_t *parent, char *contents, char *name, vfs_inode_t **output);
static ssize_t ext2_readlink(vfs_inode_t *inode, char *buffer, size_t maxlen);
static int ext2_rename(vfs_inode_t *parent, vfs_inode_t *child, char *child_name, vfs_inode_t *dst_dir, char *dst_name, unsigned int flags);
static int ext2_destroy(vfs_inode_t *inode);
static int ext2_setattr(vfs_inode_t *inode, vfs_inode_attr_t *attr, uint32_t change);
vfs_inode_ops_t ext2_inode_ops = {
    .create = ext2_create,
    .destroy = ext2_destroy,
    .getattr = NULL,
    .link = ext2_link,
    .lookup = ext2_lookup,
    .mkdir = ext2_mkdir,
    .readlink = ext2_readlink,
    .rmdir = ext2_rmdir,
    .setattr = ext2_setattr,
    .symlink = ext2_symlink,
    .truncate = ext2_truncate,
    .unlink = ext2_unlink,
    .rename = ext2_rename,
};

static int ext2_read_range(vfs_inode_t *inode, page_range_t *r);
static int ext2_write_range(vfs_inode_t *inode, page_range_t *r);
vfs_cache_ops_t ext2_cache_ops = {
    .read_range = ext2_read_range,
    .write_range = ext2_write_range
};

static ext2_node_t *ext2_allocateNode(ext2_t *fs) {
    ext2_node_t *n = slab_allocate(node_cache);
    memset(n, 0, sizeof(ext2_node_t));
    RWSEM_INIT(&n->sem);
    SPINLOCK_INIT(&n->dcursor.lock);
    n->fs = fs;

    return n;
}

static void ext2_freeNode(ext2_node_t *node) {
    slab_free(node_cache, node);
}

static void ext2_nodePrepareInode(ext2_t *fs, ext2_node_t *n, vfs_inode_t *inode) {
    inode->ops = &ext2_inode_ops;
    inode->f_ops = &ext2_file_ops;
    inode->c_ops = &ext2_cache_ops;
    inode->mount = fs->mount;
    inode->priv = (void*)n;

    uint16_t t = n->ino.type_permissions;

#define TRANSLATE_TYPE(ext, vfs) if ((t & 0xF000) == (ext)) inode->attr.type = (vfs);
    TRANSLATE_TYPE(EXT2_INODE_TYPE_FIFO, VFS_PIPE);
    TRANSLATE_TYPE(EXT2_INODE_TYPE_CHARDEV, VFS_CHARDEVICE);
    TRANSLATE_TYPE(EXT2_INODE_TYPE_DIR, VFS_DIRECTORY);
    TRANSLATE_TYPE(EXT2_INODE_TYPE_BLKDEV, VFS_BLOCKDEVICE);
    TRANSLATE_TYPE(EXT2_INODE_TYPE_FILE, VFS_FILE);
    TRANSLATE_TYPE(EXT2_INODE_TYPE_SYMLINK, VFS_SYMLINK);
    TRANSLATE_TYPE(EXT2_INODE_TYPE_SOCKET, VFS_SOCKET);
#undef TRANSLATE_TYPE


    if (inode->attr.type != VFS_FILE) {
        inode->c_ops = NULL;
    }

    inode->attr.mode = 0;

#define TRANSLATE_PERM(p) if (t & EXT2_INODE_PERM_##p) inode->attr.mode |= S_I##p;
    TRANSLATE_PERM(XOTH); TRANSLATE_PERM(WOTH); TRANSLATE_PERM(ROTH);
    TRANSLATE_PERM(XGRP); TRANSLATE_PERM(WGRP); TRANSLATE_PERM(RGRP);
    TRANSLATE_PERM(XUSR); TRANSLATE_PERM(WUSR); TRANSLATE_PERM(RUSR);
    TRANSLATE_PERM(SVTX); TRANSLATE_PERM(SGID); TRANSLATE_PERM(SUID);
#undef TRANSLATE_PERM

    inode->attr.size = ext2_size(n->fs, &n->ino);
    inode->attr.atime = n->ino.atime;
    inode->attr.mtime = n->ino.mtime;
    inode->attr.ctime = n->ino.ctime;
    inode->attr.uid = n->ino.uid;
    inode->attr.gid = n->ino.gid;
    inode->attr.nlink = n->ino.nlink;
    inode->attr.blkcnt = n->ino.disk_sectors;
    inode->attr.ino = n->num;
    if (inode->attr.type == VFS_CHARDEVICE || inode->attr.type == VFS_BLOCKDEVICE) {
        inode->attr.rdev = n->ino.dbp[0] ? n->ino.dbp[0] : n->ino.dbp[1];
    }
}

static void ext2_nodeInitFresh(ext2_node_t *n, mode_t mode, vfs_ino_type_t type) {
    memset(&n->ino, 0, sizeof(ext2_inode_t));

#define TRANSLATE_TYPE(ext, vfs) if (type == (vfs)) n->ino.type_permissions |= (ext);
    TRANSLATE_TYPE(EXT2_INODE_TYPE_FIFO, VFS_PIPE);
    TRANSLATE_TYPE(EXT2_INODE_TYPE_CHARDEV, VFS_CHARDEVICE);
    TRANSLATE_TYPE(EXT2_INODE_TYPE_DIR, VFS_DIRECTORY);
    TRANSLATE_TYPE(EXT2_INODE_TYPE_BLKDEV, VFS_BLOCKDEVICE);
    TRANSLATE_TYPE(EXT2_INODE_TYPE_FILE, VFS_FILE);
    TRANSLATE_TYPE(EXT2_INODE_TYPE_SYMLINK, VFS_SYMLINK);
    TRANSLATE_TYPE(EXT2_INODE_TYPE_SOCKET, VFS_SOCKET);
#undef TRANSLATE_TYPE

#define TRANSLATE_PERM(p) if (mode & S_I##p) n->ino.type_permissions |= EXT2_INODE_PERM_##p;
    TRANSLATE_PERM(XOTH); TRANSLATE_PERM(WOTH); TRANSLATE_PERM(ROTH);
    TRANSLATE_PERM(XGRP); TRANSLATE_PERM(WGRP); TRANSLATE_PERM(RGRP);
    TRANSLATE_PERM(XUSR); TRANSLATE_PERM(WUSR); TRANSLATE_PERM(RUSR);
    TRANSLATE_PERM(SVTX); TRANSLATE_PERM(SGID); TRANSLATE_PERM(SUID);
#undef TRANSLATE_PERM


    n->ino.atime = n->ino.mtime = n->ino.ctime = VFS_NOW();
    n->ino.uid = current_cpu->current_process->cred.uid;
    n->ino.gid = current_cpu->current_process->cred.gid;
    n->ino.nlink = (type == VFS_DIRECTORY) ? 2 : 1;
    n->dirty = true;
    n->last_alloc = 0;
}

int ext2_rootInit(vfs_mount_t *mount, ext2_t *fs) {
    // this is called on fs init
    ext2_node_t *n = ext2_allocateNode(fs);

    int r = ext2_inodeRead(fs, EXT2_ROOT_INODE, &n->ino);
    if (r != 0) {
        ext2_freeNode(n);
        return r;
    }

    n->num = EXT2_ROOT_INODE;

    // Prepare the inode
    vfs_inode_t *inode = vfs_inode();
    ext2_nodePrepareInode(fs, n, inode);
    mount->root = inode;

    return 0;
}

void ext2_nodeInit() {
    node_cache = slab_createCache("ext2 node cache", SLAB_CACHE_DEFAULT, sizeof(ext2_node_t), 0, NULL, NULL);
}

static int ext2_insertNode(vfs_inode_t *parent, char *name, mode_t mode, vfs_ino_type_t type, dev_t dev, vfs_inode_t **output, ext2_node_t **nodeout) {
    ext2_node_t *dir = parent->priv;
    ext2_t *fs = dir->fs;

    ino_t ino;
    int r = ext2_allocInode(fs, EXT2_INO_GROUP(fs, dir->num), (type == VFS_DIRECTORY), &ino);
    if (r != 0) {
        return r;
    }

    ext2_node_t *n = ext2_allocateNode(fs);
    n->num = ino;
    ext2_nodeInitFresh(n, mode, type);

    if (type == VFS_CHARDEVICE || type == VFS_BLOCKDEVICE) {
        // encode device number
        unsigned major = ((dev >> 8) & 0xfff) | ((dev >> 32) & ~0xfffULL);
        unsigned minor = (dev & 0xff) | ((dev >> 12) & ~0xffULL);
        if (major < 256 && minor < 256) n->ino.dbp[0] = (major << 8) | minor;
        else n->ino.dbp[1] = (uint32_t)dev;
    }

    r = ext2_flushNode(n);
    if (r != 0) {
        // TODO call ext2_freeInode
        LOG(ERR, "Failed to flush inode (error code %d)\n", r);
        ext2_markError(fs);
        ext2_freeNode(n);
        return r;
    }

    // Insert into directory
    NODE_START_WRITE(dir);
    r = ext2_dirInsert(dir, name, ino, type);
    if (r == 0) {
        parent->attr.size = ext2_size(fs, &dir->ino);
        if (ext2_flushNode(dir) != 0) ext2_markError(fs);
    }
    NODE_FINISH_WRITE(dir);

    if (r != 0) {
        // TODO call ext2_freeInode
        LOG(ERR, "ext2_dirInsert failed with error code %d\n", r);
        ext2_markError(fs);
        ext2_freeNode(n);
        return r;
    }

    // Finish it up
    if (nodeout) *nodeout = n;
    if (!output) return 0;

    // Create the inode
    vfs_inode_t *i = vfs_iget(parent->mount, ino);
    if ((i->state & INODE_STATE_NEW) == 0) {
        // this should be impossible...
        BUG("Inode %lu was already cached on create???", ino);
    }

    ext2_nodePrepareInode(fs, n, i);
    vfs_createdInode(i);
    *output = i;
    return 0;
}

/**** VFS ****/

static int ext2_open(vfs_file_t *file, unsigned long flags) {
    file->priv = file->inode->priv;
    return 0;
}

static int ext2_get_entries(vfs_file_t *file, vfs_dir_context_t *ctx) {
    ext2_node_t *n = file->priv;

    NODE_START_READ(n);
    int r = ext2_dirIterate(n, ctx);
    NODE_FINISH_READ(n);

    if (r != 0) {
        return r;
    }

    if (!EXT2_HAS_DIRENT_TYPE(n->fs)) {
        // dirIterate doesn't take this into account but the dtype bit in the dirent
        // is completely invalid
        // TODO: read the inodes and calculate ctx->type that way
        assert(0 && "no support for no dirent type");
    }

    return 0;
}

static int ext2_lookup(vfs_inode_t *inode, char *name, vfs_inode_t **out) {
    ext2_node_t *dir = inode->priv;
    ext2_t *fs = dir->fs;

    NODE_START_READ(dir);
    ino_t ino;
    int r = ext2_dirFind(dir, name, &ino);
    NODE_FINISH_READ(dir);

    if (r != 0) return r;

    // Try to get the inode cached
    vfs_inode_t *i = vfs_iget(inode->mount, ino);
    if ((i->state & INODE_STATE_NEW) == 0) {
        *out = i;
        return 0;
    }

    // Create a new node
    ext2_node_t *n = ext2_allocateNode(fs);

    // Read the inode and repare it
    r = ext2_inodeRead(fs, ino, &n->ino);
    if (r != 0) {
        ext2_freeNode(n);
        inode_release(i);
        return r;
    }

    n->num = ino;
    ext2_nodePrepareInode(fs, n, i);
    vfs_createdInode(i);

    *out = i;
    return 0;
}

static int ext2_create(vfs_inode_t *parent, char *name, mode_t mode, vfs_inode_t **output) {
    return ext2_insertNode(parent, name, mode, VFS_FILE, 0, output, NULL);
}

static int ext2_mkdir(vfs_inode_t *parent, char *name, mode_t mode, vfs_inode_t **output) {
    ext2_node_t *n = parent->priv;
    ext2_t *fs = n->fs;

    // create the directory
    ext2_node_t *child;
    int r = ext2_insertNode(parent, name, mode, VFS_DIRECTORY, 0, output, &child);
    if (r != 0) {
        return r;
    }

    // add the . and .. entries
    NODE_START_WRITE(child);
    r = ext2_dirInsert(child, ".", child->num, VFS_DIRECTORY);
    if (r == 0) {
        r = ext2_dirInsert(child, "..", parent->attr.ino, VFS_DIRECTORY);
    }
    if (r != 0) ext2_markError(fs);
    NODE_FINISH_WRITE(child);

    // update the linkage
    NODE_START_WRITE(n);
    n->ino.nlink++;
    parent->attr.nlink = n->ino.nlink;
    n->dirty = true;
    if (ext2_flushNode(n) != 0) ext2_markError(fs);
    NODE_FINISH_WRITE(n);

    (*output)->attr.size = ext2_size(fs, &child->ino);
    return r;
}

static int ext2_unlink(vfs_inode_t *inode, vfs_inode_t *child, char *name) {
    ext2_node_t *dir = inode->priv;
    ext2_node_t *c = child->priv;
    ext2_t *fs = dir->fs;
    NODE_START_WRITE(c);

    // make sure this directory is empty
    bool is_dir = (child->attr.type == VFS_DIRECTORY);
    if (is_dir) {
        bool empty = false;
        int r = ext2_dirEmpty(c, &empty);
        if (r != 0) {
            NODE_FINISH_WRITE(c);
            return r;
        }

        if (!empty) {
            NODE_FINISH_WRITE(c);
            return -ENOTEMPTY;
        }
    }

    // dirs have 2 links
    nlink_t lnk = is_dir ? 2 : 1;
    if (c->ino.nlink >= lnk) c->ino.nlink -= lnk;
    else c->ino.nlink = 0;

    // finished, flush node and check
    c->dirty = true;
    nlink_t rem = c->ino.nlink;
    if (ext2_flushNode(c) != 0) ext2_markError(fs);
    NODE_FINISH_WRITE(c);

    // Inode attribute needs to be updated as well
    child->attr.nlink = rem;

    // Now to update the parent
    NODE_START_WRITE(dir);

    int r = ext2_dirRemove(dir, name, c->num);
    if (r != 0) {
        ext2_markError(fs);
        NODE_FINISH_WRITE(dir);
        return r;
    }

    // If there's nothing left update nlink on parent too
    if (rem == 0 && is_dir) {
        dir->ino.nlink--;
        inode->attr.nlink = dir->ino.nlink;
        dir->dirty = true;
    }

    if (ext2_flushNode(dir) != 0) ext2_markError(fs);
    NODE_FINISH_WRITE(dir);
    return 0;
}

static int ext2_rmdir(vfs_inode_t *inode, vfs_inode_t *child, char *name) {
    return ext2_unlink(inode, child, name);
}

static int ext2_link(vfs_inode_t *inode, vfs_inode_t *parent, char *link_name) {
    ext2_node_t *n = inode->priv;
    ext2_node_t *dir = parent->priv;
    ext2_t *fs = n->fs;

    NODE_START_WRITE(n);
    n->ino.nlink++;
    n->dirty = true;
    if (ext2_flushNode(n) != 0) ext2_markError(fs);
    inode->attr.nlink = n->ino.nlink;
    NODE_FINISH_WRITE(n);


    NODE_START_WRITE(dir);
    int r = ext2_dirInsert(dir, link_name, n->num, inode->attr.type);
    if (r != 0) {
        NODE_FINISH_WRITE(dir);
        ext2_markError(fs);
        return r;
    }

    parent->attr.size = ext2_size(dir->fs, &dir->ino);
    if (ext2_flushNode(dir) != 0) ext2_markError(fs);
    NODE_FINISH_WRITE(dir);

    return r;
}

static int ext2_symlink(vfs_inode_t *parent, char *contents, char *name, vfs_inode_t **output) {
    ext2_node_t *p = parent->priv;
    ext2_t *fs = p->fs;

    // create the directory
    ext2_node_t *child;
    int r = ext2_insertNode(parent, name, 0777, VFS_SYMLINK, 0, output, &child);
    if (r != 0) {
        return r;
    }

    // write the link data
    NODE_START_WRITE(child);

    size_t len = strlen(contents);
    if (len < EXT2_EMBEDDED_LINK_SIZE) {
        memcpy(child->ino.dbp, contents, len);
        ext2_setSize(fs, &child->ino, len);
        child->dirty = true;
    } else {
        ssize_t ret = ext2_nodeWriteBytes(child, 0, len, contents);
        if (ret != (ssize_t)len) {
            ext2_markError(fs);
            NODE_FINISH_WRITE(child);
            return (int)ret;
        }
    }

    if (ext2_flushNode(child) != 0) ext2_markError(fs);

    (*output)->attr.size = ext2_size(fs, &child->ino);
    NODE_FINISH_WRITE(child);

    return 0;
}

static ssize_t ext2_readlink(vfs_inode_t *inode, char *buffer, size_t maxlen) {
    if (!maxlen) return 0;

    ext2_node_t *n = inode->priv;
    ext2_t *fs = n->fs;

    NODE_START_READ(n);
    size_t len = ext2_size(fs, &n->ino);
    if (len > maxlen - 1) len = maxlen - 1;

    ssize_t r = (ssize_t)len;
    if (len) {
        if (ext2_size(fs, &n->ino) < EXT2_EMBEDDED_LINK_SIZE) {
            memcpy(buffer, n->ino.dbp, len);
        } else {
            r = ext2_nodeReadBytes(n, 0, len, buffer);
        }
    }

    NODE_FINISH_READ(n);

    if (r < 0) return r;
    buffer[r] = 0;
    return r;
}

// stupid helpers since LOCK_BOTH wont work for this
static void ext2_lockDirs(ext2_node_t *a, ext2_node_t *b) {
    if (a == b) {
        NODE_START_WRITE(a);
    } else if (a->num < b->num) {
        NODE_START_WRITE(a);
        NODE_START_WRITE(b);
    } else {
        NODE_START_WRITE(b);
        NODE_START_WRITE(a);
    }
}

static void ext2_unlockDirs(ext2_node_t *a, ext2_node_t *b) {
    if (a == b) {
        NODE_FINISH_WRITE(a);
    } else if (a->num < b->num) {
        NODE_FINISH_WRITE(b);
        NODE_FINISH_WRITE(a);
    } else {
        NODE_FINISH_WRITE(a);
        NODE_FINISH_WRITE(b);
    }
}

static int ext2_rename(vfs_inode_t *parent, vfs_inode_t *child, char *child_name, vfs_inode_t *dst_dir, char *dst_name, unsigned int flags) {
    ext2_node_t *src = parent->priv;
    ext2_node_t *dst = dst_dir->priv;
    ext2_node_t *c = child->priv;
    ext2_t *fs = src->fs;

    // if there's already something at the destination, it might be a victim that we should overwrite
    // victims can't be overwritten if they are directories and have contents
    vfs_inode_t *victim = NULL;
    int r = vfs_lookupat(dst_dir, dst_name, &victim, LOOKUP_NO_FOLLOW);
    if (r != 0 && r != -ENOENT) return r;
    if (r == -ENOENT) victim = NULL;

    bool is_dir = child->attr.type == VFS_DIRECTORY;
    if (victim) {
        if (victim == child) {
            // ballright bro
            inode_release(victim);
            return 0;
        }

        if (victim->mount != dst_dir->mount || (victim->flags & INODE_FLAG_MOUNTPOINT)) {
            inode_release(victim);
            return -EBUSY;
        }

        if ((victim->attr.type == VFS_DIRECTORY) != is_dir) {
            inode_release(victim);
            return is_dir ? -ENOTDIR : -EISDIR;
        }

        if (is_dir) {
            ext2_node_t *vn = victim->priv;
            bool empty;

            NODE_START_READ(vn);
            r = ext2_dirEmpty(vn, &empty);
            NODE_FINISH_READ(vn);

            if (r != 0 || !empty) {
                inode_release(victim);
                return r != 0 ? r : -ENOTEMPTY;
            }
        }
    }

    // Lock directories
    ext2_lockDirs(src, dst);

    ino_t actual;
    r = ext2_dirFind(src, child_name, &actual);
    if (r != 0 || actual != c->num) {
        r = -ENOENT;
        goto _unlock;
    }

    // find the destination again for race check and more stuff
    r = ext2_dirFind(dst, dst_name, &actual);
    if (victim) {
        // sanity check
        if (r != 0 || actual != victim->attr.ino) {
            r = -EAGAIN;
            goto _unlock;
        }
    } else if (r == 0) {
        r = -EEXIST;
        goto _unlock;
    } else if (r != -ENOENT) {
        goto _unlock;
    }

    // replace entry
    r = ext2_dirInsert(dst, dst_name, c->num, child->attr.type);
    if (r != 0) goto _unlock;
    if (victim) {
        r = ext2_dirRemove(dst, dst_name, victim->attr.ino);
        if (r != 0) {
            ext2_markError(fs);
            goto _unlock;
        }
    }

    // remove the old entry
    r = ext2_dirRemove(src, child_name, c->num);
    if (r != 0) {
        ext2_markError(fs);
        goto _unlock;
    }

    // update link counting of course
    if (is_dir && src != dst) {
        if (src->ino.nlink) src->ino.nlink--;
        dst->ino.nlink++;
    }

    if (victim && is_dir && dst->ino.nlink) {
        dst->ino.nlink--;
    }

    // timing
    uint32_t now = VFS_NOW();
    src->ino.mtime = src->ino.ctime = now;
    dst->ino.mtime = dst->ino.ctime = now;
    src->dirty = true;
    dst->dirty = true;

    parent->attr.size = ext2_size(fs, &src->ino);
    dst_dir->attr.size = ext2_size(fs, &dst->ino);
    parent->attr.nlink = src->ino.nlink;
    dst_dir->attr.nlink = dst->ino.nlink;

_unlock:
    if (ext2_flushNode(src) != 0) ext2_markError(fs);
    if (src != dst && ext2_flushNode(dst) != 0) ext2_markError(fs);
    ext2_unlockDirs(src, dst);

    // if it was a dirty, the dir needs to be flushed and updated
    if (r == 0 && is_dir && src != dst) {
        NODE_START_WRITE(c);
        int err = ext2_dirSetParent(c, dst->num);
        if (err != 0) ext2_markError(fs);
        c->ino.ctime = VFS_NOW();
        c->dirty = true;
        if (ext2_flushNode(c) != 0) ext2_markError(fs);
        NODE_FINISH_WRITE(c);
    }

    if (victim) {
        if (r == 0) {
            ext2_node_t *vn = victim->priv;
            nlink_t links = is_dir ? 2 : 1;

            NODE_START_WRITE(vn);
            vn->ino.nlink = vn->ino.nlink > links ? vn->ino.nlink - links : 0;
            vn->ino.ctime = VFS_NOW();
            vn->dirty = true;
            victim->attr.nlink = vn->ino.nlink;
            victim->attr.ctime = vn->ino.ctime;
            if (ext2_flushNode(vn) != 0) ext2_markError(fs);
            NODE_FINISH_WRITE(vn);
        }

        // release the victim's old reference
        if (r == 0 && victim->attr.nlink == 0) inode_release(victim);
        inode_release(victim);
    }

    return r;
}

static int ext2_destroy(vfs_inode_t *inode) {
    ext2_node_t *n = inode->priv;
    int r = 0;
    NODE_START_WRITE(n);

    if (inode->attr.nlink == 0) {
        // does it need to be truncated to 0? (if symlink it only needs 0 trunc once size grows past builtin)
        if (inode->attr.type == VFS_FILE || inode->attr.type == VFS_DIRECTORY || (inode->attr.type == VFS_SYMLINK && ext2_size(n->fs, &n->ino) >= 60)) {
            r = ext2_truncateInternal(n, 0);
            if (r != 0) goto _finish;
        }

        n->ino.dtime = VFS_NOW();
        n->dirty = true;
        r = ext2_flushNode(n);
        if (r != 0) goto _finish;
        r = ext2_freeInode(n->fs, n->num, inode->attr.type == VFS_DIRECTORY);
    } else {
        r = ext2_flushNode(n);
    }

_finish:
    if (r != 0) ext2_markError(n->fs);
    NODE_FINISH_WRITE(n);

    inode->priv = NULL;
    ext2_freeNode(n);
    return r;
}

static int ext2_setattr(vfs_inode_t *inode, vfs_inode_attr_t *attr, uint32_t change) {
    ext2_node_t *n = inode->priv;
    NODE_START_WRITE(n);

    if (change & INODE_ATTR_CHANGE_MODE) {
        n->ino.type_permissions = (n->ino.type_permissions & 0xF000) | (attr->mode & 07777);
        inode->attr.mode = attr->mode & 07777;
    }

    if (change & INODE_ATTR_CHANGE_UID) {
        n->ino.uid = attr->uid;
        inode->attr.uid = attr->uid;
    }

    if (change & INODE_ATTR_CHANGE_GID) {
        n->ino.gid = attr->gid;
        inode->attr.gid = attr->gid;
    }

    if (change & INODE_ATTR_CHANGE_ATIME) n->ino.atime = inode->attr.atime = attr->atime;
    if (change & INODE_ATTR_CHANGE_MTIME) n->ino.mtime = inode->attr.mtime = attr->mtime;
    if (change & INODE_ATTR_CHANGE_CTIME) n->ino.ctime = attr->ctime;
    else if (change & (INODE_ATTR_CHANGE_MODE | INODE_ATTR_CHANGE_UID | INODE_ATTR_CHANGE_GID)) n->ino.ctime = VFS_NOW();

    inode->attr.ctime = n->ino.ctime;
    n->dirty = true;

    ext2_flushNode(n);
    NODE_FINISH_WRITE(n);
    return 0;
}

static int ext2_truncate(vfs_inode_t *inode, size_t size) {
    if (inode->attr.type != VFS_FILE) return -EINVAL;

    ext2_node_t *n = inode->priv;
    NODE_START_WRITE(n);
    int r = ext2_truncateInternal(n, size);
    if (r != 0) {
        NODE_FINISH_WRITE(n);
        return r;
    }

    n->ino.mtime = VFS_NOW();
    inode->attr.size = ext2_size(n->fs, &n->ino);
    inode->attr.blkcnt = n->ino.disk_sectors;
    ext2_flushNode(n);

    NODE_FINISH_WRITE(n);
    return 0;
}

static int ext2_fsync(vfs_file_t *file) {
    ext2_node_t *n = file->priv;

    if (file->inode->cache) {
        cache_syncInode(file->inode);
    }

    NODE_START_WRITE(n);
    int r = ext2_flushNode(n);
    NODE_FINISH_WRITE(n);

    return r;
}

/**** RANGE ****/

static void ext2_zeroRange(page_range_t *r, size_t pos, size_t len) {
    while (len) {
        size_t page = pos / PAGE_SIZE;
        size_t off = pos % PAGE_SIZE;
        size_t chunk = min(len, PAGE_SIZE-off);

        uintptr_t p = arch_mmu_remap_physical(r->pages[page], PAGE_SIZE, REMAP_TEMPORARY);
        memset((void*)(p + off), 0, chunk);
        arch_mmu_unmap_physical(p, PAGE_SIZE);

        pos += chunk;
        len -= chunk;
    }
}

static void ext2_copyRange(page_range_t *r, size_t pos, char *buffer, size_t len, bool into) {
    while (len) {
        size_t page = pos / PAGE_SIZE;
        size_t off = pos % PAGE_SIZE;
        size_t chunk = min(len, PAGE_SIZE - off);

        uintptr_t p = arch_mmu_remap_physical(r->pages[page], PAGE_SIZE, REMAP_TEMPORARY);
        if (into) memcpy((void*)(p + off), buffer, chunk);
        else memcpy(buffer, (void*)(p + off), chunk);
        arch_mmu_unmap_physical(p, PAGE_SIZE);

        pos += chunk;
        buffer += chunk;
        len -= chunk;
    }
}

// this is a little silly helper
// runs that are not page aligned, i.e. those that are starting on an odd block in an fs with blksize < PAGE_SIZE
// must be page-aligned for cache operations on them. so we use a bounce page to move it
static int ext2_bounce(ext2_t *fs, page_range_t *r, size_t pos, loff_t disk, size_t len, bool write) {
    uintptr_t page = pmm_allocatePage(ZONE_DEFAULT);

    char *tmp = (char*)arch_mmu_remap_physical(page, PAGE_SIZE, REMAP_TEMPORARY);
    int ret = 0;

    while (len) {
        loff_t aligned = disk & ~((loff_t)PAGE_SIZE - 1);
        size_t skip = (size_t)(disk - aligned);
        size_t chunk = min(len, PAGE_SIZE - skip);

        if (write && (skip || chunk != PAGE_SIZE)) {
            ret = ext2_bio(fs, aligned, &page, 1, false);
            if (ret != 0) break;
        } else if (!write) {
            ret = ext2_bio(fs, aligned, &page, 1, false);
            if (ret != 0) break;
        }

        if (write) {
            ext2_copyRange(r, pos, tmp + skip, chunk, false);
            ret = ext2_bio(fs, aligned, &page, 1, true);
            if (ret != 0) break;
        } else {
            ext2_copyRange(r, pos, tmp + skip, chunk, true);
        }

        disk += chunk;
        pos += chunk;
        len -= chunk;
    }

    arch_mmu_unmap_physical((uintptr_t)tmp, PAGE_SIZE);
    pmm_freePage(page);
    return ret;
}

static int ext2_read_range(vfs_inode_t *inode, page_range_t *r) {
    ext2_node_t *n = inode->priv;
    ext2_t *fs = n->fs;

    NODE_START_READ(n);

    // first calculate the actual available bytes to read
    uint64_t fsize = ext2_size(fs, &n->ino);
    size_t total = r->npages * PAGE_SIZE;
    size_t valid = 0;
    if ((uint64_t)r->offset < fsize) {
        valid = (size_t)min((uint64_t)total, fsize - (uint64_t)r->offset);
    }

    size_t pos = 0;
    int ret = 0;
    while (pos < valid) {
        loff_t foff = r->offset + pos;
        uint32_t lblk = (uint32_t)(foff >> fs->blkshift);

        size_t remaining = valid - pos;
        uint32_t want = EXT2_TO_BLOCKS(fs, remaining);

        // get an extent for it
        ext2_extent_t ext;
        ret = ext2_map(n, lblk, want, false, &ext);
        if (ret != 0) break;

        size_t ebyte = (size_t)ext.len << fs->blkshift;
        size_t run = min(remaining, ebyte);

        if (!ext.pblk) {
            // hole in the range, reads as zero
            ext2_zeroRange(r, pos, run);
            pos += run;
            continue;
        }

        // read em all
        loff_t disk = EXT2_BLOCK_TO_DISK(fs, ext.pblk);
        size_t avail = min(ebyte, total - pos);

        if ((pos % PAGE_SIZE) == 0 && (disk % PAGE_SIZE) == 0 && avail >= PAGE_SIZE) {
            // this is page aligned, just DMA it in
            size_t npg = avail / PAGE_SIZE;

            ret = ext2_bio(fs, disk, &r->pages[pos / PAGE_SIZE], npg, false);
            if (ret != 0) break;

            pos += npg * PAGE_SIZE;
            continue;
        }

        // damn this is not page aligned, bounce it
        ret = ext2_bounce(fs, r, pos, disk, run, false);
        if (ret != 0) break;

        pos += run;
    }

    // everything past EOF is 0
    if (ret == 0 && valid < total) {
        ext2_zeroRange(r, valid, total - valid);
    }

    NODE_FINISH_READ(n);
    return ret;
}

static int ext2_write_range(vfs_inode_t *inode, page_range_t *r) {
    ext2_node_t *n = inode->priv;
    ext2_t *fs = n->fs;

    NODE_START_WRITE(n);

    // the VFS should call truncate() before using this ufnction
    loff_t limit = (loff_t)EXT2_TO_BLOCKS(fs, ext2_size(fs, &n->ino)) << fs->blkshift;
    if (r->offset > limit) {
        NODE_FINISH_WRITE(n);
        return 0;
    }

    // write loop, general idea is the same as read_range:
    // get as many pages as possible into an extent, then write out one extent at a time, bouncing if nonaligned
    size_t total = (size_t)min((uint64_t)(r->npages * PAGE_SIZE), limit - (uint64_t)r->offset);
    size_t pos = 0;
    int ret = 0;
    while (pos < total) {
        uint32_t lblk = (uint32_t)((r->offset + pos) >> fs->blkshift);
        uint32_t want = EXT2_TO_BLOCKS(fs, total - pos);

        // try to get an extent for as many blocks as we can write
        ext2_extent_t ext;
        ret = ext2_map(n, lblk, want, true, &ext);
        if (ret != 0) {
            goto _finish;
        }
        
        size_t run = min(total - pos, (size_t)ext.len << fs->blkshift);
        loff_t disk = EXT2_BLOCK_TO_DISK(fs, ext.pblk);

        // if its page aligned, we can directly submit it to ext2_bio(), else a bounce is needed
        if ((pos % PAGE_SIZE) == 0 && (disk % PAGE_SIZE) == 0 && run >= PAGE_SIZE) {
            size_t npages = run / PAGE_SIZE;
            ret = ext2_bio(fs, disk, &r->pages[pos / PAGE_SIZE], npages, true);
            if (ret != 0) goto _finish;

            pos += npages * PAGE_SIZE;
        } else {
            ret = ext2_bounce(fs, r, pos, disk, run, true);
            if (ret != 0) break;
            pos += run;
        }
    }

_finish:
    n->ino.mtime = VFS_NOW();
    n->dirty = true;
    ext2_flushNode(n);

    NODE_FINISH_WRITE(n);
    return ret;
}

