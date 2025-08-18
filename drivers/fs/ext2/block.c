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

/**
 * @brief Write a block to an EXT2 filesystem
 * @param ext2 The filesystem to write
 * @param block The block number to write
 * @param buffer Buffer location
 * @returns Error code
 */
int ext2_writeBlock(ext2_t *ext2, uint32_t block, uint8_t *buffer) {
    ssize_t error = fs_write(ext2->drive, block * ext2->block_size, ext2->block_size, buffer);
    if (error != ext2->block_size) {
        return error;
    }

    return 0;
}

/**
 * @brief Allocate a new block in an EXT2 filesystem
 * @param ext2 The filesystem to allocate a block in
 */
uint32_t ext2_allocateBlock(ext2_t *ext2) {
    uint32_t bgd = 0;
    uint8_t *bg_buffer = NULL;
    uint32_t bg_offset = 0;
    int found = 0;

    for (; bgd < ext2->bgd_count; bgd++) {
        if (ext2->bgds[bgd].unallocated_blocks) {
            // We have some unallocated blocks
            int r = ext2_readBlock(ext2, ext2->bgds[bgd].block_usage_bitmap, &bg_buffer);
            if (r) return 0x0; // Error

            // Find it
            bg_offset = 0;
            while (bg_buffer[(bg_offset >> 3)] & (1 << (bg_offset % 8))) {
                bg_offset++;
                if (bg_offset >= ext2->superblock.bg_block_count) break;
            }   

            if (bg_offset >= ext2->superblock.bg_block_count) {
                LOG(WARN, "Corrupted BGD: %d (could not find a free block)\n", bgd);
                continue;
            }

            // We found a free block, break
            found = 1;
            break;
        }
    }

    if (!found) return 0x0;

    // Set the bit in the buffer
    bg_buffer[(bg_offset >> 3)] |= (1 << (bg_offset % 8));
    ext2_writeBlock(ext2, ext2->bgds[bgd].block_usage_bitmap, bg_buffer);
    kfree(bg_buffer);

    ext2->superblock.unallocated_blocks--;
    ext2->bgds[bgd].unallocated_blocks--;
    ext2_flushSuperblock(ext2);
    ext2_flushBGDs(ext2);
    return bg_offset + (bgd * ext2->superblock.bg_block_count) + 1;
}