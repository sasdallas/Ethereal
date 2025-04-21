/**
 * @file hexahedron/include/kernel/fs/vfs.h
 * @brief VFS header file
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of Ethereal Operating System.
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
#include <bits/dirent.h>
#include <sys/types.h>
#include <structs/tree.h>


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

typedef int (*mkdir_t)(struct fs_node *, char *, mode_t);
typedef int (*unlink_t)(struct fs_node *, char *);
typedef int (*readlink_t)(struct fs_node *, char *, size_t);
typedef int (*ioctl_t)(struct fs_node*, unsigned long, void *);
typedef int (*symlink_t)(struct fs_node*, char *, char *);


// Inode structure
typedef struct fs_node {
    char name[256];         // Node name (max of 256)
    mode_t mask;            // Permissions mask
    uid_t uid;              // User ID
    gid_t gid;              // Group ID

    uint64_t flags;         // Node flags (e.g. VFS_SYMLINK)
    uint64_t inode;         // Device-specific, provides a way for the filesystem to idenrtify files
    uint64_t length;        // Size of file
    uint64_t impl;          // Implementation-defined number

    // Times
    time_t atime;           // Access timestamp (last access time)
    time_t mtime;           // Modified timestamp (last modification time)
    time_t ctime;           // Change timestamp (last metadata change time)

    // Functions
    read_t read;            // Read function
    write_t write;          // Write function
    open_t open;            // Open function
    close_t close;          // Close function
    readdir_t readdir;      // Readdir function
    finddir_t finddir;      // Finddir function
    // Create is not included - it can be done with open(O_CREAT) 
    mkdir_t mkdir;          // Mkdir function
    unlink_t unlink;        // Unlink function
    ioctl_t ioctl;          // I/O control function
    readlink_t readlink;    // Readlink function
    symlink_t symlink;      // Symlink function

    // Last file stuff
    struct fs_node *ptr;    // Used by mountpoints and symlinks
    int64_t refcount;       // Reference count on the file
    void *dev;              // Device structure
} fs_node_t;

// Hexahedron uses a mount callback system that works similar to interrupt handlers.
// Filesystems will register themselves with vfs_registerFilesystem() and provide a mount callback.
// Then when the user wants to mount something, all they have to do is call vfs_mountFilesystem which
// will try all possible filesystems on the drive.
// Note that vfs_mountFilesystem, vfs_mountFilesystemType, and vfs_mount are not the same

typedef struct fs_node* (*mount_callback)(char *argument, char *mountpoint); // Argument can be whatever, it's fs-specific.

typedef struct vfs_filesystem {
    char *name;             // Name of the filesystem
    mount_callback mount;   // Mount callback
} vfs_filesystem_t;

// We also use custom tree nodes for each VFS entry.
// This is a remnant of legacy Ethereal Operating System that I liked - it allows us to know what filesystem type is assigned to what node.
// It also allows for a root node to not immediately be mounted.
typedef struct vfs_tree_node {
    char *name;         // Yes, node->name exists but this is faster and allows us to have "mapped" nodes
                        // that do nothing but can point to other nodes (e.g. /device/)
    char *fs_type;
    fs_node_t *node;
} vfs_tree_node_t;


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
 * @brief Make directory
 * @param path The path of the directory
 * @param mode The mode of the directory created
 * @returns Error code
 */
int fs_mkdir(char *path, mode_t mode);

/**
 * @brief Unlink file
 * @param name The name of the file to unlink
 * @returns Error code
 */
int fs_unlink(char *name);

/**
 * @brief I/O control file
 * @param node The node to ioctl
 * @param request The ioctl request to use
 * @param argp Arguments to ioctl call
 * @returns Error code
 */
int fs_ioctl(fs_node_t *node, unsigned long request, char *argp);

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
int vfs_registerFilesystem(char *name, mount_callback mount);

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
 * @brief Dump VFS tree system
 */
void vfs_dump();

#endif