/**
 * @file hexahedron/fs/ramdev.c
 * @brief Creates blocks of RAM as filesystem objects
 * 
 * @warning Usage of ramdevs is usually bad and doesn't turn out well. 
 * @note A ramdev can only be created by calling @c ramdev_mount
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <kernel/fs/ramdev.h>
#include <kernel/mem/mem.h>
#include <kernel/mem/alloc.h>
#include <kernel/debug.h>
#include <string.h>

/* Last RAM index - this is used to have multiple devices */
int ram_index = 0;


ssize_t ramdev_read(fs_node_t *node, off_t offset, size_t size, uint8_t *buffer) {
    if (!size || !buffer) return 0;
    if ((size_t)offset > node->length) return 0;

    if (offset + size > node->length) {
        // Update size as it's too big.
        size = node->length - offset;
    }

    // Copy from virtual memory
    memcpy(buffer, node->dev + offset, size);

    return size; 
}

ssize_t ramdev_write(fs_node_t *node, off_t offset, size_t size, uint8_t *buffer) {
    if (!size || !buffer) return 0;
    if ((size_t)offset > node->length) return 0;

    if (offset + size > node->length) {
        // Update size as it's too big.
        size = node->length - offset;
    }

    // Write to virtual memory
    memcpy(node->dev + offset, buffer, size);

    return size; 
}


/**
 * @brief Mount RAM device
 * @param addr Virtual memory to use
 * @param size The size of the ramdev
 */
fs_node_t *ramdev_mount(uintptr_t addr, uintptr_t size) {
    page_t *pg = mem_getPage(NULL, addr, MEM_DEFAULT);
    if (!pg || !PAGE_IS_WRITABLE(pg))  {
        dprintf(WARN, "Failed to create RAM device - requires read/write page\n");
        return NULL;
    }

    fs_node_t *node = (fs_node_t*)kmalloc(sizeof(fs_node_t));
    memset(node, 0, sizeof(fs_node_t));

    snprintf(node->name, 10, "ram%i", ram_index);
    node->flags = VFS_BLOCKDEVICE;
    node->read = ramdev_read;
    node->write = ramdev_write;
    node->length = size;
    node->mask = 0700; // Owner only (allow groups?)
    node->dev = (void*)addr;

    char path[64];
    snprintf(path, 63, "/device/%s", node->name);
    vfs_mount(node, path);

    ram_index++;

    return node;
}

