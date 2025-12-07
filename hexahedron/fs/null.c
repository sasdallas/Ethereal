/**
 * @file hexahedron/fs/null.c
 * @brief Null and zero device
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */


#include <kernel/fs/null.h>
#include <kernel/fs/vfs.h>
#include <kernel/mm/alloc.h>
#include <string.h>
#include <kernel/init.h>

/**
 * @brief Null read
 */
ssize_t nulldev_read(fs_node_t *node, off_t offset, size_t size, uint8_t *buffer) {
    return size;
}

/**
 * @brief Null write
 */
ssize_t nulldev_write(fs_node_t *node, off_t offset, size_t size, uint8_t *buffer) {
    return size;
}

/**
 * @brief Zero read
 */
ssize_t zerodev_read(fs_node_t *node, off_t offset, size_t size, uint8_t *buffer) {
    memset(buffer, 0, size);
    return size;
}

/**
 * @brief Zedro write
 */
ssize_t zerodev_write(fs_node_t *node, off_t offset, size_t size, uint8_t *buffer) {
    return size;
}

/**
 * @brief Initialize the null device and mounts it
 */
int null_init() {
    // Allocate nulldev
    fs_node_t *nulldev = fs_node();

    // Setup parameters
    strncpy(nulldev->name, "null", 256);
    nulldev->read = nulldev_read;
    nulldev->write = nulldev_write;
    nulldev->mask = 0666;
    nulldev->flags = VFS_CHARDEVICE;

    // Mount the devices
    vfs_mount(nulldev, NULLDEV_MOUNT_PATH);
    return 0;
}

/**
 * @brief Initialize the zero device and mount it
 */
int zero_init() {
    // Allocate zerodev
    fs_node_t *zerodev = fs_node();

    // Setup parameters
    strncpy(zerodev->name, "zero", 256);
    zerodev->read = zerodev_read;
    zerodev->write = zerodev_write;
    zerodev->mask = 0666;
    zerodev->flags = VFS_CHARDEVICE;

    // Mount the devices
    vfs_mount(zerodev, ZERODEV_MOUNT_PATH);
    return 0;
}

FS_INIT_ROUTINE(null, INIT_FLAG_DEFAULT, null_init);
FS_INIT_ROUTINE(zero, INIT_FLAG_DEFAULT, zero_init);