/**
 * @file hexahedron/include/kernel/fs/tmpfs.h
 * @brief Temporary in-memory filesystem
 * 
 * Slow implementation
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef KERNEL_FS_TEMPFS_H
#define KERNEL_FS_TEMPFS_H

/**** INCLUDES ****/
#include <kernel/fs/vfs_new.h>
#include <stdint.h>
#include <structs/hashmap.h>

/**** TYPES ****/

typedef struct tmpfs_node {
    struct tmpfs_node *parent;          // Parent
    mutex_t lck;                        // Lock
    ino_t ino;                          // Inode number
    union {
        struct {
            uintptr_t *page_list;       // Page array
            size_t page_count;          // How many pages were allocated for the tmpfs
        } file;

        struct {
            hashmap_t *children;        // Fast children access
        } dir;

        struct {
            char *path;                 // Path of the symlink
        } symlink;

        struct {
            struct tmpfs_node *node_link; // Linked node
        } hardlink; 
    };

    vfs_inode_attr_t attr;              // Inode attributes
} tmpfs_node_t;





#endif