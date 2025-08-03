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

#include <kernel/fs/random.h>
#include <string.h>
#include <stdlib.h>


static ssize_t random_read(fs_node_t *node, off_t offset, size_t size, uint8_t *buffer) {
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
void random_mount() {
    fs_node_t *n = fs_node();
    strcpy(n->name, "random");
    n->mask = 0666;
    n->uid = n->gid = 0;
    n->length = 1024;
    n->flags = VFS_CHARDEVICE;
    n->read = random_read;

    vfs_mount(n, "/device/random");
}