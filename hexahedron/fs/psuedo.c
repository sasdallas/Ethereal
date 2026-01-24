/**
 * @file hexahedron/fs/psuedo.c
 * @brief Provides psuedo-devices
 * 
 * Provides:
 * /device/null
 * /device/zero
 * /device/full
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/fs/devfs.h>
#include <kernel/init.h>

static ssize_t null_read(devfs_node_t *n, loff_t off, size_t size, char *buffer);
static ssize_t null_write(devfs_node_t *n, loff_t off, size_t size, const char *buffer);
static ssize_t zero_read(devfs_node_t *n, loff_t off, size_t size, char *buffer);
static ssize_t zero_write(devfs_node_t *n, loff_t off, size_t size, const char *buffer);
static ssize_t full_read(devfs_node_t *n, loff_t off, size_t size, char *buffer);
static ssize_t full_write(devfs_node_t *n, loff_t off, size_t size, const char *buffer);

/* /device/null ops */
static devfs_ops_t null_dev_ops = {
    .open = NULL,
    .close = NULL,
    .read = null_read,
    .write = null_write,
    .ioctl = NULL
} ;

/* /device/zero ops */
static devfs_ops_t zero_dev_ops = {
    .open = NULL,
    .close = NULL,
    .read = zero_read,
    .write = zero_write,
    .ioctl = NULL,
    .poll = NULL,
};

/* /device/full ops */
static devfs_ops_t full_dev_ops = {
    .open = NULL,
    .close = NULL,
    .read = full_read,
    .write = full_write,
    .ioctl = NULL,
    .poll = NULL,
};

static ssize_t null_read(devfs_node_t *n, loff_t off, size_t size, char *buffer) { return 0; }
static ssize_t null_write(devfs_node_t *n, loff_t off, size_t size, const char *buffer) { return size; }
static ssize_t zero_read(devfs_node_t *n, loff_t off, size_t size, char *buffer) { memset(buffer, 0, size); return size; }
static ssize_t zero_write(devfs_node_t *n, loff_t off, size_t size, const char *buffer) { return size; }
static ssize_t full_read(devfs_node_t *n, loff_t off, size_t size, char *buffer) { memset(buffer, 0, size); return size; }
static ssize_t full_write(devfs_node_t *n, loff_t off, size_t size, const char *buffer) { return -ENOSPC; }

/**
 * @brief psuedo devices init
 */
int psuedo_init() {
    assert(devfs_register(devfs_root, "null", VFS_BLOCKDEVICE, &null_dev_ops, DEVFS_MAJOR_NULL, 0, NULL));
    assert(devfs_register(devfs_root, "zero", VFS_BLOCKDEVICE, &zero_dev_ops, DEVFS_MAJOR_ZERO, 0, NULL));
    assert(devfs_register(devfs_root, "full", VFS_BLOCKDEVICE, &full_dev_ops, DEVFS_MAJOR_FULL, 0, NULL));
    return 0;
}

FS_INIT_ROUTINE(psuedo, INIT_FLAG_DEFAULT, psuedo_init, devfs);