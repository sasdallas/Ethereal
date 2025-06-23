/**
 * @file hexahedron/include/kernel/fs/tmpfs.h
 * @brief Temporary filesystem manager
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_FS_TMPFS_H
#define KERNEL_FS_TMPFS_H

/**** INCLUDES ****/
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include <structs/tree.h>
#include <kernel/fs/vfs.h>
#include <kernel/misc/spinlock.h>

/**** DEFINITIONS ****/

/* File types */
#define TMPFS_FILE          0
#define TMPFS_DIRECTORY     1
#define TMPFS_SYMLINK       2

/* Block size */
#define TMPFS_BLOCK_SIZE        4096
#define TMPFS_DEFAULT_BLOCKS    16

/**** TYPES ****/

typedef struct tmpfs_file {
    spinlock_t  *lock;              // Lock for the file
    fs_node_t   *parent;            // Parent filesystem node

    // Fragmented blocks
    uintptr_t   *blocks;            // Block list
    size_t      blk_size;           // Size of block array
    size_t      blk_count;          // Amount of blocks allocated for the file
    size_t      length;             // Length
} tmpfs_file_t;

typedef struct tmpfs_entry {
    int type;               // Type of the entry
    tree_t *tree;           // Tree
    tree_node_t *tnode;     // Tree node

    // General metadata
    char    name[256];      // Name
                            // TODO: Not waste memory on this?
    int     mask;           // File mask
    time_t  atime;          // Access time
    time_t  mtime;          // Modification time
    time_t  ctime;          // Creation time
    uid_t uid;              // User ID
    gid_t gid;              // Group ID

    // Device
    tmpfs_file_t *file;     // File structure, only present on TMPFS_FILE
} tmpfs_entry_t;

/**** FUNCTIONS ****/

/**
 * @brief Initialize the temporary filesystem handler
 */
void tmpfs_init();

#endif