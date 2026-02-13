/**
 * @file hexahedron/fs/random.c
 * @brief /device/random 
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <string.h>
#include <stdlib.h>
#include <kernel/fs/devfs.h>
#include <kernel/init.h>

static ssize_t random_read(devfs_node_t *n, loff_t off, size_t size, char *buffer);
static devfs_ops_t random_dev_ops = {
    .open = NULL,
    .close = NULL,
    .read = random_read,
    .write = NULL,
    .ioctl = NULL,
    .lseek = NULL,
    .mmap = NULL,
    .mmap_prepare = NULL,
    .munmap = NULL,
    .poll_events = NULL,
    .poll = NULL,
} ;


/**
 * @brief random read
 */
static ssize_t random_read(devfs_node_t *n, loff_t off, size_t size, char *buffer) {
	size_t s = 0;
	
    while (s < size) {
        // Read from buffer
		buffer[s] = rand() % 0xFF;
		s++;
	}

	return size;
}

/**
 * @brief Mount random device
 */
static int random_mount() {
    return !devfs_register(devfs_root, "random", VFS_CHARDEVICE, &random_dev_ops, DEVFS_MAJOR_RANDOM, 0, NULL);
}

FS_INIT_ROUTINE(random, INIT_FLAG_DEFAULT, random_mount);