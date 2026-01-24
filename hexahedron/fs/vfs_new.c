/**
 * @file hexahedron/fs/vfs.c
 * @brief Virtual Filesystem (generation 2)
 *
 * Important note for VFS users:
 * When you get an inode as a result of ANY VFS function, ALWAYS use @c inode_release to release your reference on it when finished.
 * Inodes will start life with 1 reference and once unlinked they lose that starting reference.
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/fs/vfs_new.h>
#include <kernel/processor_data.h>
#include <kernel/lock/rwsem.h>
#include <kernel/debug.h>
#include <kernel/mm/vmm.h>
#include <structs/tree.h>
#include <structs/hashmap.h>
#include <kernel/mm/vmm.h>
#include <string.h>
#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 256
#endif

/* Log method */
#define LOG(status, ...) dprintf_module(status, "FS:VFS2", __VA_ARGS__)

/* UNCOMMENT TO ENABLE DEBUG */
#define VFS_DEBUG 1

/**
 * We will store all of our mounts in vfs_map, with the hashmap
 * being parent inode -> vfs_mount_entry_t linked list.
 */
typedef struct vfs_mount_entry {
    struct vfs_mount_entry *next;
    char child_name[NAME_MAX];
    vfs_mount_t *mount;
    vfs_inode_t *mountpoint;
    vfs_inode_t *child;
} vfs_mount_entry_t;

/* Main VFS tree */
mutex_t vfs_map_mut = MUTEX_INITIALIZER;
static hashmap_t *vfs_map = NULL;

/* Caches */
slab_cache_t *inode_cache = NULL;
slab_cache_t *file_cache = NULL;

/* Root */
static vfs_inode_ops_t dummy_ops = {NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL};
vfs_inode_t *vfs_root_inode = NULL; // Initialized as a dummy node in vfs_init

/* Filesystem map */
hashmap_t *vfs_fs_map = NULL;

/* Next inode */
/* Used by memory things like tmpfs/initfs */
static atomic_long vfs_next_ino = 1;

/**
 * @brief Initialize VFS
 * 
 * Create the caches and setup the filesystem map. Initializes a dummy node before initfs.
 */
void VFS_PREFIX(init)() {
    LOG(INFO, "VFS2 initializing\n");

    inode_cache = slab_createCache("vfs inode cache", SLAB_CACHE_DEFAULT, sizeof(vfs_inode_t), 0, NULL, NULL);
    file_cache = slab_createCache("vfs file cache", SLAB_CACHE_DEFAULT, sizeof(vfs_file_t), 0, NULL, NULL);
    vfs_map = hashmap_create_int("vfs map", 20);
    vfs_fs_map = hashmap_create("vfs filesystem map", 10);

    // Initialize dummy node
    vfs_root_inode = vfs2_inode();
    vfs_root_inode->ops = &dummy_ops;
    inode_hold(vfs_root_inode);
}

/**
 * @brief Get the last component of a path
 * On / returns nothing.
 */
static void vfs_pathLast(char *path_in, char **last_out) {
    char *p = path_in + (strlen(path_in) - 1);
    if (p == path_in) { *last_out = p; return; }

    p--;
    while (*p && p >= path_in) {
        if (*p == '/') {
            break;
        }

        p--;
    }

    // go back up
    p++;
    *last_out = p;
}


/**
 * @brief Canonicalize a virtual filesystem path
 * @param wd The working directory of the process
 * @param input The input path
 * @param output The output path
 * @returns 0 on success. If result path is more than PATH_MAX will return -EINVAL 
 * 
 * All paths returned by this function will not end in a "/", directory or file.
 */
int vfs_canonicalize(char *wd, char *input, char *output) {
    size_t total_len = strlen(input) + (wd ? strlen(wd) : 0) + 2;
    char temporary[total_len];

    if (!wd || *input == '/') {
        strncpy(temporary, input, total_len);
    } else {
        snprintf(temporary, total_len, "%s/%s", wd, input);
    }

    // Tokenize the path
    char *stack[50]; // TODO: 50?
    int stk_pos = 0;

    char *save;
    char *pch = strtok_r(temporary, "/", &save);
    while (pch) {
        if (*pch == 0 || (*pch == '.' && *(pch+1) == 0)) {
            // Either nothing or a .
        } else if (!strcmp(pch, "..")) {
            if (stk_pos) stk_pos--;
        } else {
            stack[stk_pos++] = pch;
            if (stk_pos >= 50) return -EINVAL;
        }

        pch = strtok_r(NULL, "/", &save);
    }

    // Now, rebuild the path.
    int pos = 0;
    *output = '/'; *(output+1) = 0; pos++;
    for (int i = 0; i < stk_pos; i++) {
        size_t n = strlen(stack[i]);
        if (n + pos + (i+1 != stk_pos) + 1 >= PATH_MAX) return -EINVAL;
        memcpy(&output[pos], stack[i], n);   
        pos += n;
        if (i+1 != stk_pos) output[pos++] = '/';
        output[pos] = 0;
    }

    // Clear trailing slash
    if (pos > 1 && output[pos-1] == '/') {
        output[pos-1] = 0;
    }

    // LOG(DEBUG, "Canon finished: %s\n", output);

    return 0;
}

/**
 * @brief Internal lookup
 * 
 * Inode comes out locked
 */
static int __lookupat(vfs_inode_t *inode, char *name, vfs_inode_t **output, uint32_t flags) {
    // TODO: the actual bulk of this function is SUPPOSED to search the inode's dentry cache.. but I haven't made it yet.
    mutex_acquire(&vfs_map_mut);    // Yes we hold this for longer than we should, it's probably fine.
                                    // I don't see a reason to refcount/lock the vfs_mount_entry_t 

    vfs_mount_entry_t *m = hashmap_get(vfs_map, inode);
    if (m) {
        // a mountpoint is present for this inode, but we still don't know whether the mountpoint is for us or not
        // Search for the child
        while (m) {
            if (!strcmp(m->child_name, name)) {
                // Mount for the child
                if (flags & LOOKUP_NO_MOUNTS) { 
                    mutex_release(&vfs_map_mut); 
                    return 123; // stupid magic val to let unlink/rmdir know this is a mount and it was violated :( 
                }

                mutex_release(&vfs_map_mut);
                *output = m->mountpoint;
                inode_hold(m->mountpoint);
                return 0;
            }

            m = m->next;
        }
    }
    mutex_release(&vfs_map_mut);
    
    // Not a mount, keep going
    vfs_inode_t *out_tmp = NULL;
    int res = inode_lookup(inode, name, &out_tmp);
    if (res != 0) return res;

    if (out_tmp->attr.type == VFS_SYMLINK && (flags & LOOKUP_NO_FOLLOW) == 0) {
        // Follow the path
        // TODO: this can infinitely call itself
        
        char path[PATH_MAX];
        ssize_t sz = vfs_readlink(out_tmp, path, PATH_MAX);
        if (sz < 0) { inode_release(out_tmp); return sz; }
        path[sz] = 0;

        // LOG(DEBUG, "Follow symlink %s\n", path);

        vfs_inode_t *out_again_tmp = NULL;
        res = vfs_lookup(path, &out_again_tmp, flags);
        if (res) { inode_release(out_tmp); return res; }
        *output = out_again_tmp;
        inode_release(out_tmp);
        return 0;
    }

    // Lock the inode
    // The inode comes out of lookup locked
    *output = out_tmp;
    return 0;
}

/**
 * @brief Lookup a path name in the VFS
 * @param inode The parent inode to lookup at
 * @param name The single-component name to lookup
 * @param output The output inode
 * @param flags Lookup flags (LOOKUP_xxx)
 * @note The inode comes out LOCKED. When you are finished, release it with @c inode_unlock
 * @returns 0 on success, anything else is an error code.
 */
int vfs_lookupat(vfs_inode_t *inode, char *name, vfs_inode_t **output, uint32_t flags) {
    if (!inode) {
        // // Ok bro
        // if (*name != '/') {
        //     assert(current_cpu->current_process && current_cpu->current_process->wd_node);
        //     return vfs_lookupat(current_cpu->current_process->wd_node, name, output, flags);
        // }

        // return vfs_lookupat(vfs_root_inode, name, output, flags);
        return vfs_lookup(name, output, flags); // !!!: maybe inf. recursion? 
    }

    // Do path canonicalization
    char path_canon[strlen(name) + 2];
    if (vfs_canonicalize(NULL, name, path_canon)) return -EINVAL;

    // Looking up the parent
    if (flags & LOOKUP_PARENT) {
        char *p = strrchr(path_canon, '/');
        if (p) *p = 0;
    }

    // give them root if they really want it
    if (!strcmp(path_canon, "/") || *path_canon == 0) {
        *output = inode;
        inode_hold(inode);
        return 0;
    }

    // Get cur and hold it
    vfs_inode_t *cur = inode;
    inode_hold(cur);

    char *save;
    char *pch = strtok_r(path_canon, "/", &save);
    while (pch) {
        vfs_inode_t *output_tmp;

        int r = __lookupat(cur, pch, &output_tmp, flags);

        // LOG(DEBUG, "__lookupat %p %s -> %p %d\n", inode, pch, output_tmp, r);
        if (r) { inode_release(cur); return r; }
        inode_release(cur);
        cur = output_tmp;
        pch = strtok_r(NULL, "/", &save);
    }

    *output = cur; // already locked by __lookupat
    return 0;
}

/**
 * @brief VFS lookup inode
 * @param name The full path to look up the inode at
 * @param output The output inode
 * @param flags Lookup flags (LOOKUP_xxx)
 * @note The inode comes out LOCKED. When you are finished, release it with @c inode_unlock
 * @returns 0 on success, anything else is an error code.
 */
int vfs_lookup(char *path, vfs_inode_t **output, uint32_t flags) {
    // canonicalize the path
    if (current_cpu->current_process) {
        char *wd = current_cpu->current_process->wd_path;
        char canon[strlen(wd) + strlen(path) + 1];
        if (vfs_canonicalize(wd, path, canon)) {
            return -EINVAL;
        }

        return vfs_lookupat(vfs_root_inode, canon, output, flags); // yes this is stupid, no i dont give a shit.
    } else {
        return vfs_lookupat(vfs_root_inode, path, output, flags);
    }
}

/**
 * @brief Mount at specific node
 * @param filesystem The filesystem to mount
 * @param parent The parent to mount at 
 * @param src The source
 * @param dst The destination path
 * @param flags Mount flags (MS_)
 * @param data Data
 */
int vfs_mountat(vfs2_filesystem_t *filesystem, vfs_inode_t *parent, char *src, char *dst, unsigned long flags, void *data) {
    // Get the inode at dst (this also pins the inode)
    vfs_inode_t *dst_inode;
    if (vfs_lookupat(parent, dst, &dst_inode, LOOKUP_DEFAULT)) {
        return -ENOENT;
    }

    // Create a new mount
    vfs_mount_t *mount_dst = kzalloc(sizeof(vfs_mount_t));
    mount_dst->flags = flags;
    mount_dst->fs = filesystem;
    int mres = filesystem->mount(filesystem, mount_dst, src, flags, data);
    if (mres != 0) {
        // Error
        LOG(ERR, "vfs_mountat: detected mount failure due error %d\n", mres);
        kfree(mount_dst);
        inode_release(dst_inode);
        return mres;
    }

    // The filesystem was mounted successfully
    mount_dst->next = filesystem->fs_mounts;
    if (filesystem->fs_mounts) filesystem->fs_mounts->prev = mount_dst;
    filesystem->fs_mounts = mount_dst;

    // Now create the mount entry
    vfs_mount_entry_t *ent = kzalloc(sizeof(vfs_mount_entry_t));
    strncpy(ent->child_name, dst, NAME_MAX);
    ent->mountpoint = mount_dst->root;
    ent->child = dst_inode;
    ent->mount = mount_dst;
    inode_hold(ent->mountpoint);

    LOG(DEBUG, "mountpoint has %d refs\n", ent->mountpoint->refcount);

    // we will NOT drop the reference to the root inode, as it's now being referenced by ent.

    mutex_acquire(&vfs_map_mut);
    ent->next = hashmap_get(vfs_map, parent);
    LOG(DEBUG, "vfs_mountat src %s dest %s parent %p filesystem %s\n", src, dst, parent, filesystem->name);
    hashmap_set(vfs_map, parent, ent);
    mutex_release(&vfs_map_mut);
    return 0;
}

/**
 * @brief Mount specific filesystem on path
 * @param filesystem The filesystem to mount on the path
 * @param src The mount source
 * @param dst The mount destination
 * @param flags Mount flags
 * @param data Mount additional data
 * @returns 0 on success.
 */
int vfs2_mount(vfs2_filesystem_t *filesystem, char *src, char *dst, unsigned long flags, void *data) {
    if (!strncmp(dst, "/", PATH_MAX)) {
        // Wow, they just want to mount to root.
        LOG(ERR, "Cannot remount root from userspace.\n");
        return -EPERM;
    }

    // Get parent inode and child component
    vfs_inode_t *parent;
    int r = vfs_lookup(dst, &parent, LOOKUP_PARENT);
    if (r) return r;

    char *child_comp;
    vfs_pathLast(dst, &child_comp);

    LOG(DEBUG, "mounting %s to %p\n", child_comp, parent);
    return vfs_mountat(filesystem, parent, src, child_comp, flags, data); // lol, due to VFS semantics this works
}

/**
 * @brief Change the root filesystem
 * @param filesystem The new filesystem to initialize on the root.
 * @param src The source directory (mount)
 * @param flags Mount flags (mount)
 * @param data Mount data (mount)
 * 
 * This will not preserve the old root filesystem or any mounts under it, it will be deleted.
 */
int vfs_changeGlobalRoot(vfs2_filesystem_t *filesystem, char *src, unsigned long flags, void *data) {
    // TODO: I don't really know how to synchronize this very well. It is probably prone to crashing.
    // TODO: This shouldn't even be here, but I didn't make a mount namespace and I don't care enough to make one now. When a proper pivot_root is implemented, root will become process specific anyways.

    /**
     * Basically this is my dumb little thing to switch roots.
     * Linux doesn't handle switching roots very well, in my opinion (neither do I, though, so who am I to judge)
     * It has things like switch_root (which just move hardcoded mounts like /dev to the new root and then mount the new root over the old one)
     * 
     * At the current stage of this VFS I have a global root, meaning that pivot_root and others are non-optimal.
     * Instead the kernel will use this call to switch the root filesystem. This should primarily be done ONE time.
     * 
     * In the future, if someone wants to actually change the root, I'll probably just make this error out if any sub-mounts are present.
     * Something like that.
     */

    // First build the rootfs mount
    vfs_mount_t *mount_dst = kzalloc(sizeof(vfs_mount_t));
    mount_dst->flags = flags;
    mount_dst->fs = filesystem;
    int mres = filesystem->mount(filesystem, mount_dst, src, flags, data);
    if (mres != 0) {
        // Error
        kfree(mount_dst);
        return mres;
    }

    // Hold the root inode so they don't remove it 
    inode_hold(mount_dst->root);

    // !!! This doesn't lock properly. 
    mutex_acquire(&vfs_map_mut);
    vfs_inode_t *previous_inode = vfs_root_inode;
    vfs_root_inode = mount_dst->root;
    inode_release(previous_inode);
    mutex_release(&vfs_map_mut);

    return 0;
}

/**
 * @brief Unmount a path
 * @param path The path to unmount
 */
int vfs_unmount(char *path) {
    char canon_path[PATH_MAX];

    // look for it
    if (current_cpu->current_process) {
        int r = vfs_canonicalize(current_cpu->current_process->wd_path, path, canon_path);
        if (r) return r;
    } else {
        assert(*path == '/');
        int r = vfs_canonicalize(NULL, path, canon_path);
        if (r) return r;
    }

#ifdef VFS_DEBUG
    LOG(DEBUG, "Unmounting %s\n", canon_path);
#endif

    if (*canon_path == '/' && *(canon_path+1) == 0) {
        LOG(ERR, "Cannot unmount root yet - not implemented\n");
        return -ENOTSUP;
    }

    // Get the parent at the path
    vfs_inode_t *n;
    int r = vfs_lookup(canon_path, &n, LOOKUP_PARENT); // !!!: very minor waste, this runs canonicalization twice
    if (r != 0) {
        return r; 
    }

    // Now find the last part of the path.
    // Fix the end of it, since it ends with a slash.
    canon_path[strlen(canon_path)-1] = 0;
    char *p = strrchr(canon_path, '/');
    assert(p && *(p+1));
    p++;

    LOG(DEBUG, "Unmounting: %s\n", p);

    // Parent isn't pinned, no need to release a ref on it.
    // Get the mount entry
    mutex_acquire(&vfs_map_mut);
    vfs_mount_entry_t *ent = hashmap_get(vfs_map, n);
    if (!ent) { mutex_release(&vfs_map_mut); inode_release(n); return -EINVAL; } // Waste...
    vfs_mount_entry_t *last = NULL;

    while (ent) {
        if (!strncmp(ent->child_name, p, NAME_MAX)) {
            // Found the corresponding mount!

            // Perform the unmount operation if needed
            vfs_mount_t *mount = ent->mount;
            assert(mount->root == ent->mountpoint);

            if (mount->ops && mount->ops->unmount) {
                int r = mount->ops->unmount(mount);
                if (r) {
                    mutex_release(&vfs_map_mut);
                    inode_release(n);
                    return r;
                }
            }

            // Safe to unlink and release the mutex
            if (last) last->next = ent->next;
            else hashmap_set(vfs_map, n, ent->next);
            mutex_release(&vfs_map_mut);

            // Now destroy the unmount and remove it from the fs_links
            // !!!: LOCK
            if (mount->prev) mount->prev->next = mount->next;
            if (mount->next) mount->next->prev = mount->prev;

            // We held the child
            inode_release(ent->child);
            inode_release(ent->mountpoint);

            kfree(mount);
            kfree(ent);
            inode_release(n);
            return 0;
        }

        last = ent;
        ent = ent->next;
    }

    mutex_release(&vfs_map_mut);
    inode_release(n);
    return -EINVAL; // Not a mountpoint
}

/**
 * @brief Open at
 * @param inode The inode to open at (must be locked)
 * @param path The path to open
 * @param flags Opening flags (O_...)
 * @param output Output pointer for the @c vfs_file_t
 * @returns Error code
 */
int vfs_openat(vfs_inode_t *inode, char *path, long flags, vfs_file_t **output) {
    int lookup_flags = LOOKUP_DEFAULT;
    if (flags & O_NOFOLLOW) lookup_flags |= LOOKUP_NO_FOLLOW;

    vfs_inode_t *output_inode;
    int r;
    if (path) {
        // We should lookup again (lookupat comes out locked)
        r = vfs_lookupat(inode, path, &output_inode, lookup_flags);
        if (r != 0) return r;
        if (inode) inode_release(inode);
    } else {
        output_inode = inode;
        inode_hold(output_inode); // I hate inodes...
    }    

    vfs_file_t *f = vfs2_file(output_inode);
    inode_release(output_inode); // Since vfs_file locks the inode anyways we can now release the reference

    // NOTE: Assuming this is a GCC 12.2.0 bug (https://gcc.gnu.org/bugzilla/show_bug.cgi?id=107694)
#pragma GCC diagnostic ignored "-Wstringop-overflow"

    r = file_open(f, flags); // Increases the file's references too 
    if (r) {
        file_release(f); // file_open puts a reference on it regardless of failure. this will free it
        return r;    
    }

    *output = f;
    return 0;
}


/**
 * @brief VFS symlink at
 * @param inode The inode to symlink at
 * @param target Target of the link
 * @param linkpath Link path
 */
int vfs_symlinkat(vfs_inode_t *inode, char *target, char *linkpath, vfs_inode_t **symlink_out) {
    // Get the node
    vfs_inode_t *parent;
    int r = vfs_lookupat(inode, linkpath, &parent, LOOKUP_PARENT);
    if (r) return r;
    
    // Check if dir
    if (parent->attr.type != VFS_DIRECTORY) return -ENOTDIR;

    // Get child and create inode (inode_create can do EEXIST on its own)
    char *child;
    vfs_pathLast(linkpath, &child);

    vfs_inode_t *tmp;
    r = inode_symlink(parent, target, child, &tmp);
    if (r) { inode_release(parent); return r; }

    if (symlink_out) {
        inode_hold(tmp);
        *symlink_out = tmp;
    }

    if (r == 0) {
        // Now, release the inode again. This drops the initial reference when it was made by vfs_inode().
        // TODO: Add caching to store the inode
        inode_release(tmp);
    }

    inode_release(parent);
    return 0;
}

/**
 * @brief Create at
 * @param inode The inode to create at
 * @param path The path to create
 * @param mode The mode to create with
 * @param inode_output Optional inode output
 * @returns 0 on success
 */
int vfs_createat(vfs_inode_t *inode, char *path, mode_t mode, vfs_inode_t **inode_output) {
    // Get the node
    vfs_inode_t *parent;
    int r = vfs_lookupat(inode, path, &parent, LOOKUP_PARENT);
    if (r) return r;
    
    // Check if dir
    if (parent->attr.type != VFS_DIRECTORY) return -ENOTDIR;

    // Get child and create inode (inode_create can do EEXIST on its own)
    char *child;
    vfs_pathLast(path, &child);

    vfs_inode_t *tmp;
    r = inode_create(parent, child, mode, &tmp);
    if (r) { inode_release(parent); return r; }

    if (inode_output) {
        inode_hold(tmp);
        *inode_output = tmp;
    }

    if (r == 0) {
        // Now, release the inode again. This drops the initial reference when it was made by vfs_inode().
        // TODO: Add caching to store the inode
        inode_release(tmp);
    }

    inode_release(parent);
    return 0;
}

/**
 * @brief Create directory at
 * @param inode The inode to create the directory at
 * @param name The name of the directory to create
 * @param mode The mode of the directory
 * @param dirout The optional directory out
 * @returns 0 on success
 */
int vfs_mkdirat(vfs_inode_t *inode, char *name, mode_t mode, vfs_inode_t **dirout) {
    vfs_inode_t *i_parent;
    int r = vfs_lookupat(inode, name, &i_parent, LOOKUP_PARENT);
    if (r) return r;

    if (i_parent->attr.type != VFS_DIRECTORY) return -ENOTDIR;

    // get the child
    // !!!: BUG: THIS DOESNT PROTECT AGAINST CANONICALIZATION
    char *child;
    vfs_pathLast(name, &child);

    // !!!: ...
    char *p = strrchr(child, '/');
    if (p) *p = 0;  


    vfs_inode_t *holder;
    r = inode_mkdir(i_parent, child, mode, dirout ? dirout : &holder);
    if (dirout && r == 0) inode_hold(holder);
    
    if (r == 0) {
        // drop the initial reference to the inode. the inode will still be held if dirout exists.
        // TODO: add dentry caching, which will allow much faster VFS lookups 
        inode_release(holder);
    }

    inode_release(i_parent);

    if (p) *p = '/'; // restore...
    return r;
}

/**
 * @brief Read from a file
 * @param file The file to read from
 * @param off The offset to read from
 * @param size The size to read
 * @param buffer The output buffer to read into
 * @returns Amount of bytes read or negative error code
 */
ssize_t vfs_read(vfs_file_t *file, loff_t off, size_t size, char *buffer) {
    // TODO: page cache
    return file_read(file, off, size, buffer);
}

/**
 * @brief Write to a file
 * @param file The file to write to
 * @param off The offset to write to
 * @param size The size to write
 * @param buffer The output buffer to write with
 * @returns Amount of bytes write or negative error code
 */
ssize_t vfs_write(vfs_file_t *file, loff_t off, size_t size, const char *buffer) {
    // TODO: page cache
    return file_write(file, off, size, buffer);
}

/**
 * @brief Get attribute VFS
 * @param inode The inode to get the attributes of
 * @param attr The attributes output pointer
 */
int vfs_getattr(vfs_inode_t *inode, vfs_inode_attr_t *attr) {
    if (inode->ops->getattr) {
        return inode->ops->getattr(inode, attr);
    } else {
        memcpy(attr, &inode->attr, sizeof(vfs_inode_attr_t));
        return 0;
    }
}

/**
 * @brief Get an inode from the mount cache
 * @param mount The VFS mount to retrieve the inode from
 * @param ino The inode number to retrieve
 * @returns Either the cached inode, or a new inode. 
 * 
 * If the inode's state includes @c INODE_STATE_NEW then it is a brand new inode and you must configure it,
 * as it is returned locked.
 * 
 * Once you're done configuring the inode, call @c inode_finished on it to release the lock 
 * 
 * This is fine for filesystems where inode number is enough to locate the inode. Otherwise, consider using
 * alternative APIs.
 */
vfs_inode_t *vfs_iget(vfs_mount_t *mount, ino_t ino) {
    // TODO: inode cache, very bad that I don't have one
    // TODO: check all the reference counting and locking in this func

    vfs_inode_t *inode = vfs2_inode();
    spinlock_acquire(&inode->lock);
    inode->state |= INODE_STATE_NEW;
    inode_hold(inode); // Hold the inode again until it's done
    return inode;
}

/**
 * @brief Get filesystem from VFS map
 * @param name The name of the filesystem to get
 */
vfs2_filesystem_t *vfs_getFilesystem(char *name) {
    return hashmap_get(vfs_fs_map, name);
}
 
/**
 * @brief Register filesystem with the VFS
 * @param filesystem The filesystem to register
 */
void vfs_register(vfs2_filesystem_t *filesystem) {
    assert(filesystem->mount != NULL);
    hashmap_set(vfs_fs_map, (void*)filesystem->name, filesystem);
}

/**
 * @brief Unregister filesystem from the VFS
 * @param filesystem The filesystem to unregister
 */
void vfs_unregister(vfs2_filesystem_t *filesystem) {
    hashmap_remove(vfs_fs_map, (void*)filesystem->name);
}

/**
 * @brief Creates and returns a blank inode (or NULL on no memory)
 */
vfs_inode_t *VFS_PREFIX(inode)() {
    vfs_inode_t *inode = slab_allocate(inode_cache);
    if (!inode) return NULL;

    // idk
    memset(inode, 0, sizeof(vfs_inode_t));
    SPINLOCK_INIT(&inode->lock);
    refcount_init(&inode->refcount, 1); // Each inode starts life with one reference count, which is dropped shortly after create finishes.
    return inode;
}

/**
 * @brief Poll a node
 * @param f The file to poll
 * @param waiter The waiter structure to poll with
 * @param events Requested events
 * @returns IMPORTANT: Will either return 0 for "added to queue", 1 for "revents are ready", and anything else is an error.
 */
int vfs_poll(vfs_file_t *f, poll_waiter_t *waiter, poll_events_t events, poll_events_t *revents) {
    // I don't remember why I did this way, but I did...
    if (f->ops->poll_events) {
        poll_events_t ret = f->ops->poll_events(f);
        if (ret != 0 && (ret & events) != 0) {
            // early hit
            *revents = ret & events;
            return 1;
        }
    }

    if (!f->ops->poll) {
        assert(!f->ops->poll_events); // TODO this case is ambiguous and i dont care enough
        // we have to take over
        *revents = events & (POLLIN | POLLOUT);
        return 1;
    }

    // no early hit, we will be adding to queue probably
    int res = file_poll(f, waiter, events);
    assert((res == 0)); // TODO handle errors gracefully
    return 0;
}

/**
 * @brief Creates and returns a new file object
 * @param inode The inode for the file object
 */
vfs_file_t *VFS_PREFIX(file)(vfs_inode_t *inode) {
    vfs_file_t *file = slab_allocate(file_cache);
    if (!file) return NULL;

    memset(file, 0, sizeof(vfs_file_t));
    file->ops = inode->f_ops;
    refcount_init(&file->refcount, 0);
    file->inode = inode;
    inode_hold(inode);

    return file;
}


/**
 * @brief Get the next inode number
 */
ino_t vfs_getNextInode() {
    return (ino_t)atomic_fetch_add(&vfs_next_ino, 1);
}

/**
 * @brief Destroy inode
 * @param inode The inode to destroy. 
 * 
 * Should be automatically called.
 */
void vfs_destroyInode(vfs_inode_t *inode, char *fn) {
    // LOG(DEBUG, "Destroying inode %p, by %s\n", inode, fn);
    if (inode->ops->destroy) inode->ops->destroy(inode);
    slab_free(inode_cache, inode);
}

/**
 * @brief Destroy file
 * @param file The file to destroy
 * 
 * Should be automatically called
 */
void vfs_destroyFile(vfs_file_t *file) {
    // LOG(DEBUG, "Destroying file %p\n", file);
    if (file->ops && file->ops->close) file->ops->close(file);
    inode_release(file->inode);
    slab_free(file_cache, file);
}