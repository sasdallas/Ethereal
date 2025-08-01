/**
 * @file drivers/fs/ext2/block.c
 * @brief EXT2 block functions
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include "ext2.h"
#include <kernel/debug.h>
#include <kernel/mem/alloc.h>

/**
 * @brief Read a block from an EXT2 filesystem
 * @param ext2 The filesystem to read
 * @param block The block number to read
 * @param buffer Buffer location
 * @returns Error code
 */
int ext2_readBlock(ext2_t *ext2, uint32_t block, uint8_t **buffer) {
    uint8_t *bg_buffer = kmalloc(ext2->block_size);

    ssize_t error = fs_read(ext2->drive, block * ext2->block_size, ext2->block_size, bg_buffer);
    if (error != ext2->block_size) { 
        kfree(bg_buffer);
        return error;
    }

    *buffer = bg_buffer;
    return 0;
}