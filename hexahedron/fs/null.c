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
#include <kernel/mem/alloc.h>
#include <string.h>

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
void nulldev_init() {
    // Allocate nulldev
    fs_node_t *nulldev = kmalloc(sizeof(fs_node_t));
    memset(nulldev, 0, sizeof(fs_node_t));

    // Setup parameters
    strncpy(nulldev->name, "null", 256);
    nulldev->read = nulldev_read;
    nulldev->write = nulldev_write;
    nulldev->flags = VFS_CHARDEVICE;

    // Mount the devices
    vfs_mount(nulldev, NULLDEV_MOUNT_PATH);
}

/**
 * @brief Initialize the zero device and mounts it
 */
void zerodev_init() {
    // Allocate zerodev
    fs_node_t *zerodev = kmalloc(sizeof(fs_node_t));
    memset(zerodev, 0, sizeof(fs_node_t));

    // Setup parameters
    strncpy(zerodev->name, "zero", 256);
    zerodev->read = zerodev_read;
    zerodev->write = zerodev_write;
    zerodev->flags = VFS_CHARDEVICE;

    // Mount the devices
    vfs_mount(zerodev, ZERODEV_MOUNT_PATH);
}