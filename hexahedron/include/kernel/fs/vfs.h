/**
 * @file hexahedron/include/kernel/fs/vfs.h
 * @brief VFS header file
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_FS_VFS_H
#define KERNEL_FS_VFS_H

/**** INCLUDES ****/
#include <stdint.h>
#include <stddef.h>
#include <bits/types/dirent.h>
#include <sys/types.h>
#include <structs/tree.h>
#include <kernel/misc/spinlock.h>


/**** DEFINITIONS ****/

// Type bitmasks
#define VFS_FILE            0x01
#define VFS_DIRECTORY       0x02
#define VFS_CHARDEVICE      0x04
#define VFS_BLOCKDEVICE     0x08
#define VFS_PIPE            0x10
#define VFS_SYMLINK         0x20
#define VFS_MOUNTPOINT      0x40
#define VFS_SOCKET          0x80

// Event types for ready()
#define VFS_EVENT_READ      0x01
#define VFS_EVENT_WRITE     0x02
#define VFS_EVENT_ERROR     0x04

/**** TYPES ****/

// Node prototype
struct fs_node;

// These are the types of operations that can be performed on an inode.
// Sourced from the POSIX standard (tweaked to use fs_node rather than fd)
typedef void (*open_t)(struct fs_node*, unsigned int oflag); // oflag can be sourced from fcntl.h
typedef void (*close_t)(struct fs_node*);
typedef ssize_t (*read_t)(struct fs_node *, off_t, size_t, uint8_t*);
typedef ssize_t (*write_t)(struct fs_node *, off_t, size_t, uint8_t*);

typedef struct dirent* (*readdir_t)(struct fs_node *, unsigned long);
typedef struct fs_node* (*finddir_t)(struct fs_node *, char *);

typedef struct fs_node* (*create_t)(struct fs_node *, char *, mode_t);
typedef int (*mkdir_t)(struct fs_node *, char *, mode_t);
typedef int (*unlink_t)(struct fs_node *, char *);
typedef int (*readlink_t)(struct fs_node *, char *, size_t);
typedef int (*ioctl_t)(struct fs_node*, unsigned long, void *);
typedef int (*symlink_t)(struct fs_node*, char *, char *);
typedef int (*mmap_t)(struct fs_node*, void *, size_t, off_t);
typedef int (*msync_t)(struct fs_node*, void *, size_t, off_t);
typedef int (*munmap_t)(struct fs_node*, void *, size_t, off_t);
typedef int (*ready_t)(struct fs_node*, int event_type);

// Map context for a file
typedef struct vfs_mmap_context {
    struct process *proc;
    void *addr;
    size_t size;
    off_t off;
} vfs_mmap_context_t;

// Inode structure
typedef struct fs_node {
    // General information
    char name[256];             // Node name (max of 256)
    mode_t mask;                // Permissions mask
    uid_t uid;                  // User ID
    gid_t gid;                  // Group ID

    // Flags
    uint64_t flags;             // Node flags (e.g. VFS_SYMLINK)
    uint64_t inode;             // Device-specific, provides a way for the filesystem to idenrtify files
    uint64_t length;            // Size of file
    uint64_t impl;              // Implementation-defined number

    // Times
    time_t atime;               // Access timestamp (last access time)
    time_t mtime;               // Modified timestamp (last modification time)
    time_t ctime;               // Change timestamp (last metadata change time)

    // Functions
    read_t read;                // Read function
    write_t write;              // Write function
    open_t open;                // Open function
    close_t close;              // Close function
    readdir_t readdir;          // Readdir function
    finddir_t finddir;          // Finddir function
    create_t create;            // Create function
    mkdir_t mkdir;              // Mkdir function
    unlink_t unlink;            // Unlink function
    ioctl_t ioctl;              // I/O control function
    readlink_t readlink;        // Readlink function
    symlink_t symlink;          // Symlink function
    mmap_t mmap;                // Mmap function
    msync_t msync;              // Msync function
    munmap_t munmap;            // Munmap function
    ready_t ready;              // Ready function

    // Other
    list_t *mmap_contexts;      // mmap() context list
    spinlock_t waiter_lock;     // The waiter lock
    list_t *waiting_nodes;      // Waiting nodes
    struct fs_node *ptr;        // Used by mountpoints and symlinks
    int64_t refcount;           // Reference count on the file
    void *dev;                  // Device structure
} fs_node_t;

// Hexahedron uses a mount callback system that works similar to interrupt handlers.
// Filesystems will register themselves with vfs_registerFilesystem() and provide a mount callback.
// Then when the user wants to mount something, all they have to do is call vfs_mountFilesystemType with the type to use.

/**
 * @brief VFS mount callback
 * @param argument Argument provided to VFS function
 * @param mountpoint Wherever you want to mount the system
 */
typedef struct fs_node* (*vfs_mount_callback_t)(char *argument, char *mountpoint); // Argument can be whatever, it's fs-specific.

typedef struct vfs_filesystem {
    char *name;                     // Name of the filesystem
    vfs_mount_callback_t mount;     // Mount callback
} vfs_filesystem_t;

// We also use custom tree nodes for each VFS entry.
// This is a remnant of legacy reduceOS that I liked - it allows us to know what filesystem type is assigned to what node.
// It also allows for a root node to not immediately be mounted.
typedef struct vfs_tree_node {
    char *name;         // Yes, node->name exists but this is faster and allows us to have "mapped" nodes
                        // that do nothing but can point to other nodes (e.g. /device/)
    char *fs_type;
    fs_node_t *node;
} vfs_tree_node_t;

// Waiter structure for any processes waiting
typedef struct vfs_waiter {
    struct thread *thread;
    int events;
} vfs_waiter_t;

/**** FUNCTIONS ****/

/**
 * @brief Standard POSIX open call
 * @param node The node to open
 * @param flags The open flags
 */
void fs_open(fs_node_t *node, unsigned int flags);

/**
 * @brief Standard POSIX close call
 * @param node The node to close
 */
void fs_close(fs_node_t *node);

/**
 * @brief Standard POSIX read call
 * @param node The node to read from
 * @param offset The offset to read at
 * @param size The amount of bytes to read
 * @param buffer The buffer to store the bytes in
 * @returns The amount of bytes read
 */
ssize_t fs_read(fs_node_t *node, off_t offset, size_t size, uint8_t *buffer);

/**
 * @brief Standard POSIX write call
 * @param node The node to write to
 * @param offset The offset to write at
 * @param size The amount of bytes to write
 * @param buffer The buffer of the bytes to write
 * @returns The amount of bytes written
 */
ssize_t fs_write(fs_node_t *node, off_t offset, size_t size, uint8_t *buffer);

/**
 * @brief Read directory
 * @param node The node to read the directory of
 * @param index The index to read from
 * @returns A directory entry structure or NULL
 */
struct dirent *fs_readdir(fs_node_t *node, unsigned long index);

/**
 * @brief Find directory
 * @param node The node to find the path in
 * @param name The name of the file to look for
 * @returns The node of the file found or NULL
 */
fs_node_t *fs_finddir(fs_node_t *node, char *path);

/**
 * @brief Create new entry
 * @param node The node to run create() on
 * @param name The name of the new entry to create
 * @param mode The mode
 */
fs_node_t *fs_create(fs_node_t *node, char *name, mode_t mode);

/**
 * @brief mmap() a file. This is done either via the VFS' internal method or the file's
 * @param file The file to map
 * @param addr The address that was allocated for mmap()
 * @param size The size of the mapping created
 * @param off The offset of the file
 * @returns Error code
 */
int fs_mmap(fs_node_t *node, void *addr, size_t size, off_t off);

/**
 * @brief msync() a file
 * @param file The file to sync
 * @param addr The address that was allocated by mmap()
 * @param size The size of the mapping created
 * @param off The offset of the file
 */
int fs_msync(fs_node_t *node, void *addr, size_t size, off_t off);

/**
 * @brief munmap a file
 * @param file The file to munmap()
 * @param addr The address that was allocated by mmap()
 * @param size The size of the mapping created
 * @param off The offset of the file
 */
int fs_munmap(fs_node_t *node, void *addr, size_t size, off_t off);

/**
 * @brief I/O control file
 * @param node The node to ioctl
 * @param request The ioctl request to use
 * @param argp Arguments to ioctl call
 * @returns Error code
 */
int fs_ioctl(fs_node_t *node, unsigned long request, char *argp);

/**
 * @brief Check if file is ready
 * @param node The node to check
 * @param event_type OR bitmask of events (VFS_EVENT_...) to check
 * @returns OR bitmask of events the file is ready for
 */
int fs_ready(fs_node_t *node, int event_type);

/**
 * @brief Alert any processes in the queue that new events are ready
 * @param node The node to alert on
 * @param events The events to alert on
 * @returns 0 on success
 */
int fs_alert(fs_node_t *node, int events);

/**
 * @brief Wait for a node to have events ready for a process
 * @param node The node to wait for events on
 * @param events The events that are being waited for
 * @returns 0 on success
 * 
 * @note Does not actually put you to sleep. Instead puts you in the queue. for sleeping
 */
int fs_wait(fs_node_t *node, int events);

/**
 * @brief Destroy a filesystem node immediately
 * @param node The node to destroy
 * @warning This does not check if the node has references, just use @c fs_close if you don't know what you're doing
 */
void fs_destroy(fs_node_t *node);

/**
 * @brief Create and return a filesystem object
 * @note This API is relatively new and may not be in use everywhere
 * 
 * Does not initialize the refcount of the node. Open it somewhere,.
 */
fs_node_t *fs_node();

/**
 * @brief Create a copy of a filesystem node object
 * @param node The node to copy
 * @returns A new node object
 */
fs_node_t *fs_copy(fs_node_t *node);

/**
 * @brief Make directory
 * @param path The path of the directory
 * @param mode The mode of the directory created
 * @returns Error code
 */
int vfs_mkdir(char *path, mode_t mode);

/**
 * @brief Unlink file
 * @param name The name of the file to unlink
 * @returns Error code
 */
int vfs_unlink(char *name);

/**
 * @brief creat() equivalent for VFS
 * @param node The node to output to
 * @param path The path to create
 * @param mode The mode to create with
 * @returns Error code
 * 
 * @warning This logic would much better fit into the @c kopen function with @c O_CREAT
 * @warning Some more details of this are garbage, such as errno returning with @c fs_create
 */
int vfs_creat(fs_node_t **node, char *path, mode_t mode);

/**
 * @brief Initialize the virtual filesystem with no root node.
 */
void vfs_init();

/**
 * @brief Mount a specific node to a directory
 * @param node The node to mount
 * @param path The path to mount to
 * @returns A pointer to the tree node or NULL
 */
tree_node_t *vfs_mount(fs_node_t *node, char *path);

/**
 * @brief Register a filesystem in the hashmap
 * @param name The name of the filesystem
 * @param fs_callback The callback to use
 */
int vfs_registerFilesystem(char *name, vfs_mount_callback_t mount);

/**
 * @brief Try to mount a specific filesystem type
 * @param name The name of the filesystem
 * @param argp The argument you wish to provide to the mount method (fs-specific)
 * @param mountpoint Where to mount the filesystem (leave as NULL if the driver takes care of this)
 * @returns The node created or NULL if it failed
 */
fs_node_t *vfs_mountFilesystemType(char *name, char *argp, char *mountpoint);

/**
 * @brief Kernel open method 
 * @param path The path of the file to open (always absolute in kmode)
 * @param flags The opening flags - see @c fcntl.h
 * @returns A pointer to the file node or NULL if it couldn't be found
 */
fs_node_t *kopen(const char *path, unsigned int flags);

/**
 * @brief Kernel open method for usermode (uses current process' working directory)
 * @param path The path of the file to open
 * @param flags The opening flags - see @c fcntl.h
 * @returns A pointer to the file node or NULL if it couldn't be found
 */
fs_node_t *kopen_user(const char *path, unsigned int flags);

/**
 * @brief Canonicalize a path based off a CWD and an addition.
 * 
 * This basically will turn /home/blah (CWD) + ../other_directory/gk (addition) into
 * /home/other_directory/gk
 * 
 * @param cwd The current working directory
 * @param addition What to add to the current working directory
 */
char *vfs_canonicalizePath(char *cwd, char *addition);

/**
 * @brief Unmount a path from the filesystem
 * @param path The path to unmount
 * @returns 0 on success or error code
 */
int vfs_unmount(char *path);

/**
 * @brief Dump VFS tree system
 */
void vfs_dump();

#endif