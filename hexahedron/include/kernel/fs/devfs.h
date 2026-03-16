/**
 * @file hexahedron/include/kernel/fs/devfs.h
 * @brief Device filesystem
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef KERNEL_FS_DEVFS_H
#define KERNEL_FS_DEVFS_H

/**** INCLUDES ****/
#include <stdint.h>
#include <sys/types.h>
#include <kernel/fs/vfs_new.h>
#include <structs/hashmap.h>
#include <assert.h>

/**** DEFINITIONS ****/

#define DEVFS_MAJOR_RAM             1   // RAM device
#define DEVFS_MAJOR_IDE_HD          2   // IDE hard-drive
#define DEVFS_MAJOR_CDROM           3   // CDROM
#define DEVFS_MAJOR_SATA            4   // SATA drive
#define DEVFS_MAJOR_SCSI            5   // SCSI drive
#define DEVFS_MAJOR_SCSI_CDROM      6   // SCSI CD-ROM drive
#define DEVFS_MAJOR_NVME            7   // NVMe storage
#define DEVFS_MAJOR_FLOPPY          8   // Floppy disk
#define DEVFS_MAJOR_MMC             9   // MMC
#define DEVFS_MAJOR_TTY             10  // TTY device (master/slave)
#define DEVFS_MAJOR_NULL            11  // /device/null
#define DEVFS_MAJOR_FULL            12  // /device/full
#define DEVFS_MAJOR_ZERO            13  // /device/zero
#define DEVFS_MAJOR_RANDOM          14  // /device/urandom
#define DEVFS_MAJOR_NETWORK         15  // Network
#define DEVFS_MAJOR_KEYBOARD        16  // Keyboard
#define DEVFS_MAJOR_MOUSE           17  // Mouse
#define DEVFS_MAJOR_FRAMEBUFFER     18  // Framebuffer device
#define DEVFS_MAJOR_CONSOLE         19  // Console device, like /device/log or /device/fbcon
#define DEVFS_MAJOR_VIDEO           20  // Video framebuffer
#define DEVFS_MAJOR_SHARED          21  // Shared device

/**** TYPES ****/

struct devfs_node;

typedef struct devfs_ops {
    int (*open)(struct devfs_node *file, unsigned long flags);
    int (*close)(struct devfs_node *file);
    ssize_t (*read)(struct devfs_node *file, loff_t off, size_t size, char *buffer);
    ssize_t (*write)(struct devfs_node *file, loff_t off, size_t size, const char *buffer);
    int (*ioctl)(struct devfs_node *file, unsigned long request, void *argp);
    int (*lseek)(struct devfs_node *file, loff_t off, int whence, loff_t *pos); // just return the new position to set to
    poll_events_t (*poll_events)(struct devfs_node *file);
    int (*poll)(struct devfs_node *file, poll_waiter_t *waiter, poll_events_t events);
    int (*mmap)(struct devfs_node *file, void *addr, size_t len, off_t off, uint64_t flags);
    int (*mmap_prepare)(struct devfs_node *file, struct vmm_memory_range *range);
    int (*munmap)(struct devfs_node *file, void *addr, size_t len, off_t offset);
} devfs_ops_t;

typedef struct devfs_node {
    struct devfs_node *parent;  // Parent
    char *name;                 // Name of the node (required)
    devfs_ops_t *ops;           // Device operations
    dev_t major, minor;         // Major and minor numbers (DEVFS_MAJOR_..., minor is up to you)
    ino_t ino;                  // Inode number
    void *priv;                 // Private
    hashmap_t *children;        // Children list
    vfs_inode_attr_t attr;      // Attributes
    mutex_t lck;                // Mutex for children list
} devfs_node_t;

/**** VARIABLES ****/

extern devfs_node_t *devfs_root;

/**** FUNCTIONS ****/

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
devfs_node_t *devfs_register(devfs_node_t *parent, char *name, int type, devfs_ops_t *ops, dev_t major, dev_t minor, void *priv);

/**
 * @brief Create device filesystem directory
 * @param parent The parent to create the directory under
 * @param name The name of the directory
 */
devfs_node_t *devfs_createDirectory(devfs_node_t *parent, char *name);

/**
 * @brief Unregister a device node
 * @param dev The device node to unregister
 */
int devfs_unregister(devfs_node_t *node);

/**
 * @brief Get a device node by name
 * @param name The name of the device node to get
 * @returns Node or NULL
 */
devfs_node_t *devfs_get(char *name);

#endif