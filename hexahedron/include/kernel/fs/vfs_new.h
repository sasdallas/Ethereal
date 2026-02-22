/**
 * @file hexahedron/include/kernel/fs/vfs.h
 * @brief Virtual File System of the Hexahedron kernel
 * 
 * The design of this VFS is similar to that of Linux's design:
 * - A vfs_inode_t object is an inode that exists in merely the namespace of the filesystem.
 *      + Inodes implement things like lookup/create/unlink
 *      + They have attributes, in vfs_inode_attr_t
 * - A vfs_file_t object is an open file in memory.
 *      + Files are temporary, created by open() and destroyed when their refcount hits 0
 *      + Files implement things like open/read/write/ioctl/readdir
 * - A vfs_mount_t object for a mounted filesystem
 *      + The only operation for this is unmount which is done on unmount and vfs_mount_t free
 *      + Holds a root inode
 * - A vfs_filesystem_t object for a filesystem
 *      + Only used on vfs_mountFilesystem() which mounts the fs and creates a vfs_mount_t
 * - A vfs_cache_entry_t object to represent a directory entry
 *      + Directory entries are stored in lookup caches in the parent vfs_inode_t
 *      + A directory entry can reference one inode, or can reference NULL (in this case it is called a NEGATIVE dentry, where it can quickly return NULL)
 *      + Normally in your filesystem you will avoid using them.
 * 
 * The only main differences between this and a Linux VFS is that:
 *  - This is less complex.
 *  - Dentries are VFS internal.
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef KERNEL_FS_VFS2_H
#define KERNEL_FS_VFS2_H

/**** INCLUDES ****/
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <stddef.h>
#include <dirent.h>
#include <kernel/refcount.h>
#include <sys/time.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <kernel/mm/alloc.h>
#include <kernel/fs/poll.h>
#include <kernel/misc/mutex.h>
#include <kernel/debug.h>
#include <limits.h>

/**** DEFINITIONS ****/

/* Inode flags */
#define INODE_FLAG_DEFAULT          0x0 
#define INODE_FLAG_NOT_CACHEABLE    0x1     // can't cache in page cache
#define INODE_FLAG_MMAP_UNSUPPORTED 0x2     // TODO: custom error numbers on this. will probably be a silly hack.
#define INODE_FLAG_MOUNTPOINT       0x4     // this inode is the root of a mountpoint

/* Attribute set request flags */
#define INODE_ATTR_CHANGE_MODE      0x1
#define INODE_ATTR_CHANGE_ATIME     0x2
#define INODE_ATTR_CHANGE_MTIME     0x4
#define INODE_ATTR_CHANGE_CTIME     0x8
#define INODE_ATTR_CHANGE_UID       0x10
#define INODE_ATTR_CHANGE_GID       0x20

/* vfs_lookup flags */
#define LOOKUP_DEFAULT              0x0
#define LOOKUP_NO_MOUNTS            0x1     // used by unlink to avoid following mounts
#define LOOKUP_PARENT               0x2     // lookup the parent, only works on vfs_lookup() and not lookupat
#define LOOKUP_NO_FOLLOW            0x4     // do not follow symlinks

/* VFS inode states */
#define INODE_STATE_NEW             0x1

/**** TYPES ****/

struct vfs_file;
struct vfs_inode;
struct vfs_inode_attr;
struct vfs_mount;
struct vfs_filesystem;

#ifndef KERNEL_FS_VFS_H
typedef enum vfs_ino_type {
    VFS_FILE,
    VFS_DIRECTORY,
    VFS_CHARDEVICE,
    VFS_BLOCKDEVICE,
    VFS_PIPE,               // S_IFIFO
    VFS_LINK,
    VFS_SOCKET
} vfs_ino_type_t;
#define VFS_SYMLINK VFS_LINK
#endif

/* Used for the file operation get_entries */
typedef struct vfs_dir_context {
    char name[NAME_MAX];        // d_name - No need to duplicate. This will not be freed.
    unsigned int dirpos;        // Position in hte directory
    uint64_t ino;               // d_ino
    unsigned int type;          // d_type
} vfs_dir_context_t;

/* Mount operations */
typedef struct vfs_mount_ops {
    int (*unmount)(struct vfs_mount *mount);
    int (*write_inode)(struct vfs_inode *inode);
} vfs_mount_ops_t;

/* File operations */
struct vmm_memory_range;
typedef struct vfs_file_ops {
    int (*open)(struct vfs_file *file, unsigned long flags);
    int (*close)(struct vfs_file *file);
    ssize_t (*read)(struct vfs_file *file, loff_t off, size_t size, char *buffer);
    ssize_t (*write)(struct vfs_file *file, loff_t off, size_t size, const char *buffer);
    int (*ioctl)(struct vfs_file *file, long request, void *argp);
    int (*get_entries)(struct vfs_file *file, vfs_dir_context_t *ctx); // should return 1 on finished, 0 on still going, < 0 on error
    poll_events_t (*poll_events)(struct vfs_file *file); // get poll events. optional fast route.
    int (*poll)(struct vfs_file *file, poll_waiter_t *waiter, poll_events_t events); // should add waiter to their event.
    int (*mmap)(struct vfs_file *file, void *addr, size_t size, off_t off, uint64_t flags); // size will always be page aligned
    int (*munmap)(struct vfs_file *file, void *addr, size_t size, off_t offset);
    int (*mmap_prepare)(struct vfs_file *file, struct vmm_memory_range *range); // prepare the range for mapping (yes, it will be a VMM memory range)
    int (*lseek)(struct vfs_file *file, loff_t off, int whence, loff_t *pos); // optional lseek equivalent. pos is a pointer to file->pos. returns 0 on success and sets pos to the new pos.
} vfs_file_ops_t;

/* Inode attributes */
typedef struct vfs_inode_attr {
    spinlock_t attr_lock;
    vfs_ino_type_t type;
    mode_t mode;
    time_t atime;
    time_t mtime;
    time_t ctime;
    uid_t uid;
    gid_t gid;
    dev_t rdev;
    loff_t size;
    blkcnt_t blkcnt;
    ino_t ino;
    nlink_t nlink;
} vfs_inode_attr_t;


/* Inode operations */
typedef struct vfs_inode_ops {
    int (*create)(struct vfs_inode *parent, char *name, mode_t mode, struct vfs_inode **ino_output);
    int (*mkdir)(struct vfs_inode *parent, char *name, mode_t mode, struct vfs_inode **ino_output);
    int (*lookup)(struct vfs_inode *inode, char *name, struct vfs_inode **ino_output);
    int (*link)(struct vfs_inode *inode, struct vfs_inode *parent, char *link_name);
    int (*symlink)(struct vfs_inode *inode, char *link_contents, char *link_name, struct vfs_inode **sym_output);
    int (*rmdir)(struct vfs_inode *inode, const char *dir);
    int (*unlink)(struct vfs_inode *inode, struct vfs_inode *child, char *child_name);
    ssize_t (*readlink)(struct vfs_inode *inode, char *buffer, size_t maxlen);
    int (*getattr)(struct vfs_inode *inode, vfs_inode_attr_t *attr);
    int (*setattr)(struct vfs_inode *inode, vfs_inode_attr_t *attr, uint32_t attr_mask);
    int (*destroy)(struct vfs_inode *inode);
    int (*truncate)(struct vfs_inode *inode, size_t size);
    int (*rename)(struct vfs_inode *parent, struct vfs_inode *child, char *child_name, struct vfs_inode *dst_dir, char *dst_name, unsigned int flags);
} vfs_inode_ops_t;

/* VFS inode */
typedef struct vfs_inode {
    spinlock_t lock;            // Locks state
    vfs_inode_attr_t attr;      // Fast attr
    vfs_inode_ops_t *ops;       // Inode operations
    vfs_file_ops_t *f_ops;      // File operations
    int state;                  // Inode state
    struct vfs_mount *mount;    // Mountpoint
    unsigned long flags;        // Flags to the inode (INODE_FLAG_xxx)
    refcount_t refcount;        // Reference count
    void *priv;                 // Private field for the inode
} vfs_inode_t;

/* VFS file */
typedef struct vfs_file {
    vfs_inode_t *inode;         // Inode of the file
    vfs_file_ops_t *ops;        // File operations
    refcount_t refcount;        // Reference count
    loff_t pos;                 // File position
    long flags;                 // Flags
    void *priv;                 // Private data for the file
} vfs_file_t;

/* VFS mountpoint */
typedef struct vfs_mount {
    struct vfs_mount *next;     // Next in list of filesystem mounts
    struct vfs_mount *prev;     // Previous in list of filesystem mounts
    unsigned long flags;        // Mount flags
    struct vfs_filesystem *fs;  // Filesystem
    vfs_mount_ops_t *ops;       // VFS mount operations
    vfs_inode_t *root;          // Root inode
} vfs_mount_t;

/* VFS filesystem */
typedef struct vfs_filesystem {
    const char *name;           // Name
    unsigned long flags;        // Filesystem flags (not impl)
    vfs_mount_t *fs_mounts;     // Linked list of mounts

    int (*mount)(struct vfs_filesystem *filesystem, vfs_mount_t *mount_dst, char *src, unsigned long flags, void *data);
} vfs2_filesystem_t;

/**** INLINE ****/



/* Don't use these two */

void vfs_destroyFile(vfs_file_t *file);
void vfs_destroyInode(vfs_inode_t *inode, char *fn);

#if 0
static inline void inode_hold(vfs_inode_t *i) { refcount_inc(&i->refcount); }
static inline void inode_release(vfs_inode_t *i) { refcount_dec(&i->refcount); if (i->refcount == 0) { vfs_destroyInode(i); } }
#else
static inline void inode_hold(vfs_inode_t *i) { refcount_inc(&i->refcount); }
#define inode_release(i) { refcount_dec(&i->refcount); if (i->refcount == 0) { vfs_destroyInode(i, (char*)__FUNCTION__); } }
#endif

static inline void file_hold(vfs_file_t *f) { refcount_inc(&f->refcount); }
static inline void file_release(vfs_file_t *f) { refcount_dec(&f->refcount); if (f->refcount == 0) { vfs_destroyFile(f); }}



/* avoid using these, most of the time their VFS counterparts work fine */
/* note some functions are not included here such as lseek and poll_events */
static inline int file_open(vfs_file_t *f, unsigned long flags) { file_hold(f); if (f->ops->open) { return f->ops->open(f, flags); } else { return 0; } }
static inline void file_close(vfs_file_t *f) { file_release(f); }
static inline ssize_t file_read(vfs_file_t *f, loff_t off, size_t size, char *buffer) { if (f->ops->read) { return f->ops->read(f, off, size, buffer); } else { return -ENOTSUP; }}
static inline ssize_t file_write(vfs_file_t *f, loff_t off, size_t size, const char *buffer) { if (f->ops->write) { return f->ops->write(f, off, size, buffer); } else { return -ENOTSUP; }}
static inline int file_ioctl(vfs_file_t *f, long request, void *argp) { if (f->ops->ioctl) { return f->ops->ioctl(f, request, argp); } else { return -ENOTSUP; } }
static inline int file_get_entries(vfs_file_t *f, vfs_dir_context_t *ctx) { if (f->ops->get_entries) { return f->ops->get_entries(f, ctx); } else { return -ENOTSUP; }}
static inline int file_poll(vfs_file_t *f, poll_waiter_t *waiter, poll_events_t events) { if (f->ops->poll) { return f->ops->poll(f, waiter, events); } else { return -ENOTSUP; }} // ambig. if file doesn't support polling. poll()/select() should exit on file_poll_events tho
static inline int file_mmap(vfs_file_t *f, void *addr, size_t size, off_t off, uint64_t flags) { if (f->ops->mmap) { return f->ops->mmap(f, addr, size, off, flags); } else { return -ENOTSUP; }}
static inline int file_mmap_prepare(vfs_file_t *f, void *range) { if (f->ops->mmap_prepare) { return f->ops->mmap_prepare(f, range); } else { return 0; }}
static inline int file_munmap(vfs_file_t *f, void *addr, size_t size, off_t offset) { if (f->ops->munmap) { return f->ops->munmap(f, addr, size, offset); } else { return -ENOTSUP; }}
static inline int inode_create(vfs_inode_t *parent, char *name, mode_t mode, vfs_inode_t **inode_output) { if (parent->ops->create) { return parent->ops->create(parent, name, mode, inode_output); } else { return -ENOTSUP; }} // TODO: maybe EROFS?
static inline int inode_link(vfs_inode_t *i, vfs_inode_t *parent, char *link_name) { if (i->ops->link) { return i->ops->link(i, parent, link_name); } else { return -ENOTSUP; }}
static inline int inode_mkdir(vfs_inode_t *i, char *name, mode_t mode, vfs_inode_t **inode_output) { if (i->ops->mkdir) { return i->ops->mkdir(i, name, mode, inode_output); } else { return -ENOTSUP; }}
static inline int inode_symlink(vfs_inode_t *i, char *link_contents, char *link_name, vfs_inode_t **sym_output) { if (i->ops->symlink) { return i->ops->symlink(i, link_contents, link_name, sym_output); } else { return -ENOTSUP; }}
static inline int inode_rmdir(vfs_inode_t *i, const char *dir) { if (i->ops->rmdir) { return i->ops->rmdir(i, dir); } else { return -ENOTSUP; }}
static inline int inode_unlink(vfs_inode_t *i, vfs_inode_t *child, char *child_name) { if (i->ops->unlink) { return  i->ops->unlink(i, child, child_name); } else { return -ENOTSUP; }}
static inline ssize_t inode_readlink(vfs_inode_t *i, char *buffer, size_t maxlen) { if (i->ops->readlink) { return i->ops->readlink(i, buffer, maxlen); } else { return -ENOTSUP; }}
static inline int inode_getattr(vfs_inode_t *i, vfs_inode_attr_t *attr) { if (i->ops->getattr) { return i->ops->getattr(i, attr); } else { return -ENOTSUP; }}
static inline int inode_setattr(vfs_inode_t *i, vfs_inode_attr_t *attr, uint32_t attr_mask) { if (i->ops->setattr) { return i->ops->setattr(i, attr, attr_mask); } else { return -ENOTSUP; }}
static inline int inode_lookup(vfs_inode_t *i, char *name, vfs_inode_t **output) { if (i->ops->lookup) { return i->ops->lookup(i, name, output); } else { return -ENOTSUP; }}
static inline int inode_truncate(vfs_inode_t *i, size_t size) { if (i->ops->truncate) { return i->ops->truncate(i, size); } else { return -ENOTSUP; }}
static inline void inode_created(vfs_inode_t *i) { i->state &= ~(INODE_STATE_NEW); inode_release(i); spinlock_release(&i->lock); }
static inline int inode_rename(vfs_inode_t *parent, vfs_inode_t *child, char *child_name, vfs_inode_t *dst_dir, char *dst_name, unsigned int flags) { if (parent->ops->rename) { return parent->ops->rename(parent, child, child_name, dst_dir, dst_name, flags); } else { return -ENOTSUP; }}

static inline int flush_inode(vfs_inode_t *i) { if (i->mount->ops && i->mount->ops->write_inode) { return i->mount->ops->write_inode(i); } else { return 0; } }

/* these are fine to use */
static inline size_t inode_size(vfs_inode_t *i) { return i->attr.size; }; // TODO maybe getsize() callback?

/**** MACROS ****/

/* Used for the migration to the new VFS */
#define VFS_PREFIX(fn) vfs2_##fn

/* Now */
#define VFS_NOW() ({ struct timeval tv; gettimeofday(&tv, NULL); tv.tv_sec; })

/**** FUNCTIONS ****/

/**
 * @brief Initialize VFS
 */
void VFS_PREFIX(init)();

/**
 * @brief Canonicalize a virtual filesystem path
 * @param wd The working directory of the process
 * @param input The input path
 * @param output The output path
 * @returns 0 on success. If result path is more than PATH_MAX will return -EINVAL 
 */
int vfs_canonicalize(char *wd, char *input, char *output);

/**
 * @brief Lookup a path name in the VFS
 * @param inode The parent inode to lookup at
 * @param name The single-component name to lookup
 * @param output The output inode
 * @param flags Lookup flags (LOOKUP_xxx)
 * @note The inode comes out LOCKED. When you are finished, release it with @c inode_release
 * @returns 0 on success, anything else is an error code.
 */
int vfs_lookupat(vfs_inode_t *inode, char *name, vfs_inode_t **output, uint32_t flags);

/**
 * @brief VFS lookup inode
 * @param name The full path to look up the inode at
 * @param output The output inode
 * @param flags Lookup flags (LOOKUP_xxx)
 * @note The inode comes out LOCKED. When you are finished, release it with @c inode_release
 * @returns 0 on success, anything else is an error code.
 */
int vfs_lookup(char *path, vfs_inode_t **output, uint32_t flags);

/**
 * @brief Mount at specific node
 * @param filesystem The filesystem to mount
 * @param parent The parent to mount at 
 * @param src The source
 * @param dst The destination path
 * @param flags Mount flags (MS_)
 * @param data Data
 */
int vfs_mountat(vfs2_filesystem_t *filesystem, vfs_inode_t *parent, char *src, char *dst, unsigned long flags, void *data);

/**
 * @brief Mount specific filesystem on path
 * @param filesystem The filesystem to mount on the path
 * @param src The mount source
 * @param dst The mount destination
 * @param flags Mount flags
 * @param data Mount additional data
 * @returns 0 on success.
 */
int vfs2_mount(vfs2_filesystem_t *filesystem, char *src, char *dst, unsigned long flags, void *data);

/**
 * @brief Unmount a path
 * @param path The path to unmount
 */
int vfs_unmount(char *path);

/**
 * @brief Change the root filesystem
 * @param filesystem The new filesystem to initialize on the root.
 * @param src The source directory (mount)
 * @param flags Mount flags (mount)
 * @param data Mount data (mount)
 * 
 * This will not preserve the old root filesystem or any mounts under it, it will be deleted.
 */
int vfs_changeGlobalRoot(vfs2_filesystem_t *filesystem, char *src, unsigned long flags, void *data);

/**
 * @brief Create directory at
 * @param inode The inode to create the directory at
 * @param name The name of the directory to create
 * @param mode The mode of the directory
 * @param dirout The optional directory out
 * @returns 0 on success
 */
int vfs_mkdirat(vfs_inode_t *inode, char *name, mode_t mode, vfs_inode_t **dirout);

/**
 * @brief Create directory
 * @param name The name of the directory to create
 * @param mode The mode of the directory
 * @param dirout The optional directory out
 * @returns 0 on success
 */
static inline int VFS_PREFIX(mkdir)(char *name, mode_t mode, vfs_inode_t **dirout) {
    return vfs_mkdirat(NULL, name, mode, dirout);
}

/**
 * @brief Open at
 * @param inode The inode to open at (must be locked)
 * @param path The path to open
 * @param flags Opening flags (O_...)
 * @param output Output pointer for the @c vfs_file_t
 * @returns Error code
 */
int vfs_openat(vfs_inode_t *inode, char *path, long flags, vfs_file_t **output);

/**
 * @brief Open file
 * @param path The path of the file to open
 * @param flags Opening flags (O_...)
 * @param output Output pointer for the @c vfs_file_t
 * @returns Error code
 */
static inline int vfs_open(char *path, long flags, vfs_file_t **output) {
    return vfs_openat(NULL, path, flags, output);
}

/**
 * @brief Create at
 * @param inode The inode to create at
 * @param path The path to create
 * @param mode The mode to create with
 * @param inode_output Optional inode output
 * @returns 0 on success
 */
int vfs_createat(vfs_inode_t *inode, char *path, mode_t mode, vfs_inode_t **inode_output);

/**
 * @brief Create
 * @param path The path to create
 * @param mode The mode to create with
 * @param out Inode output (or leave as NULL)
 */
static inline int vfs_create(char *path, mode_t mode, vfs_inode_t **inode_output) {
    return vfs_createat(NULL, path, mode, inode_output);
}

/**
 * @brief Read from a file
 * @param file The file to read from
 * @param off The offset to read from
 * @param size The size to read
 * @param buffer The output buffer to read into
 * @returns Amount of bytes read or negative error code
 */
ssize_t vfs_read(vfs_file_t *file, loff_t off, size_t size, char *buffer);

/**
 * @brief Write to a file
 * @param file The file to write to
 * @param off The offset to write to
 * @param size The size to write
 * @param buffer The output buffer to write with
 * @returns Amount of bytes write or negative error code
 */
ssize_t vfs_write(vfs_file_t *file, loff_t off, size_t size, const char *buffer);

/**
 * @brief Close file
 * @param file The file to close
 */
static inline int vfs_close(vfs_file_t *file) {
    file_close(file);
    return 0;
}

/**
 * @brief Truncate inode
 * @param inode The inode to truncate
 * @param size The size to truncate to
 */
static inline int vfs_truncate(vfs_inode_t *inode, size_t size) {
    return inode_truncate(inode, size);
} 

/**
 * @brief Get attribute VFS
 * @param inode The inode to get the attributes of
 * @param attr The attributes output pointer
 */
int vfs_getattr(vfs_inode_t *inode, vfs_inode_attr_t *attr);

/**
 * @brief Set attribute VFS
 * @param inode The inode to get the attributes of
 * @param attr The attributes output pointer
 * @param attr_mask Mask of attributes to set
 */
static inline int vfs_setattr(vfs_inode_t *inode, vfs_inode_attr_t *attr, uint32_t attr_mask) {
    return inode_setattr(inode, attr, attr_mask);
}

/**
 * @brief VFS ioctl
 * @param file The file to perform ioctl on
 * @param request The request
 * @param argp The arguments
 */
static inline int vfs_ioctl(vfs_file_t *file, unsigned long request, void *argp) {
    return file_ioctl(file, request, argp);
}

/**
 * @brief VFS get entries
 * @param file The file to get entries of
 * @param ctx Directory context for get entries
 */
static inline int vfs_get_entries(vfs_file_t *file, vfs_dir_context_t *ctx) {
    return file_get_entries(file, ctx);
}

/**
 * @brief VFS symlink at
 * @param inode The inode to symlink at
 * @param target Target of the link
 * @param linkpath Link path
 */
int vfs_symlinkat(vfs_inode_t *inode, char *target, char *linkpath, vfs_inode_t **symlink_out);

/**
 * @brief VFS symlink
 * @param target The target of the link
 * @param linkpath The link path
 */
static inline int vfs2_symlink(char *target, char *linkpath, vfs_inode_t **symlink_out) {
    return vfs_symlinkat(NULL, target, linkpath, symlink_out);
}

/**
 * @brief VFS readlink
 * @param inode The inode to read link
 * @param buffer The destination buffer
 * @param bufsiz The size of the destination buffer
 */
static inline ssize_t vfs_readlink(vfs_inode_t *inode, char *buffer, size_t bufsiz) {
    return inode_readlink(inode, buffer, bufsiz);
}

/**
 * @brief Get filesystem from VFS map
 * @param name The name of the filesystem to get
 */
vfs2_filesystem_t *vfs_getFilesystem(char *name);

/**
 * @brief Register filesystem with the VFS
 * @param filesystem The filesystem to register
 */
void vfs_register(vfs2_filesystem_t *filesystem);

/**
 * @brief Unregister filesystem from the VFS
 * @param filesystem The filesystem to unregister
 */
void vfs_unregister(vfs2_filesystem_t *filesystem);

/**
 * @brief Creates and returns a blank inode (or NULL on no memory)
 */
vfs_inode_t *VFS_PREFIX(inode)();

/**
 * @brief Creates and returns a new file object
 * @param inode The inode for the file object
 */
vfs_file_t *VFS_PREFIX(file)(vfs_inode_t *inode);

/**
 * @brief Get the next inode number
 */
ino_t vfs_getNextInode();

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
vfs_inode_t *vfs_iget(vfs_mount_t *mount, ino_t ino);

/**
 * @brief Destroy inode
 * @param inode The inode to destroy. 
 * 
 * Should be automatically called.
 */
void vfs_destroyInode(vfs_inode_t *inode, char *fn);

/**
 * @brief Destroy file
 * @param file The file to destroy
 * 
 * Should be automatically called
 */
void vfs_destroyFile(vfs_file_t *file);

/**
 * @brief Duplicate file
 * @param f The file to duplicate
 */
static inline vfs_file_t * vfs_duplicate(vfs_file_t *file) {
    file_hold(file);
    return file;
}

/**
 * @brief Poll a node
 * @param f The file to poll
 * @param waiter The waiter structure to poll with
 * @param events Requested events
 * @returns IMPORTANT: Will either return 0 for "added to queue", 1 for "revents are ready", and anything else is an error.
 */
int vfs_poll(vfs_file_t *f, poll_waiter_t *waiter, poll_events_t events, poll_events_t *revents); 

/**
 * @brief Seek in a file
 * @param f The file to seek in
 * @param off The offset to seek to
 * @param whence Seek whence
 * @returns New offset or error code
 */
loff_t vfs_seek(vfs_file_t *file, loff_t off, int whence);

/**
 * @brief Unlink a file
 * @param inode The inode to unlink at
 * @param path The relative path to unlink
 * @returns 0 on success or error code
 */
int vfs_unlinkat(vfs_inode_t *inode, char *path);

/**
 * @brief VFS rename
 * @param src_inode The source inode to rename at
 * @param src_path The path to rename at
 * @param dst_inode The destination inode to rename to
 * @param dst_path The destination path to rename to
 * @param flags Flags for the rename operation
 */
int vfs_renameat(vfs_inode_t *src_inode, char *src_path, vfs_inode_t *dst_inode, char *dst_path, unsigned int flags);


#endif