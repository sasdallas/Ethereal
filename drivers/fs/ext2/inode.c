/**
 * @file drivers/fs/ext2/inode.c
 * @brief EXT2 inode functions
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
#include <kernel/mem/alloc.h>
#include <kernel/debug.h>
#include <errno.h>
#include <string.h>
#include <kernel/panic.h>
#include <assert.h>

/**
 * @brief Read the inode metadata from an EXT2 filesystem
 * @param ext2 The filesystem to read
 * @param inode The inode number to read
 * @param buffer The buffer location
 * @returns Error code
 */
int ext2_readInode(ext2_t *ext2, uint32_t inode, uint8_t **buffer) {
    if (!inode) return -EINVAL;
    
    // Determine the block group of the inode
    uint32_t bg = (inode - 1) / ext2->inodes_per_group;
    
    // Get the BGD
    uint32_t inode_table_block = EXT2_BGD(bg)->inode_table;

    // Get the index within that inode table
    uint32_t index = (inode - 1) % ext2->inodes_per_group;
    uint32_t offset = (index * ext2->inode_size) / ext2->block_size;
    uint32_t off2 = (index - offset * (ext2->block_size / ext2->inode_size)) * ext2->inode_size;

    // Read the block
    // TODO: Cache?
    uint8_t *buf;
    int r = ext2_readBlock(ext2, inode_table_block + offset, &buf);
    if (r) return r;

    uint8_t *inode_buf = kmalloc(ext2->inode_size);
    memcpy(inode_buf, buf + off2, ext2->inode_size);

    // Set buffer
    *buffer = inode_buf;
    kfree(buf);

    return 0;
}


/**
 * @brief Write an inode structure
 * @param ext2 EXT2 filesystem
 * @param inode The inode structure to write
 * @param inode_number The inode number
 */
int ext2_writeInode(ext2_t *ext2, ext2_inode_t *inode, uint32_t inode_number) {
    if (!inode) return -EINVAL;
    
    // Determine the block group of the inode
    uint32_t bg = (inode_number - 1) / ext2->inodes_per_group;
    
    // Get the BGD
    uint32_t inode_table_block = EXT2_BGD(bg)->inode_table;

    // Get the index within that inode table
    uint32_t index = (inode_number - 1) % ext2->inodes_per_group;
    uint32_t offset = (index * ext2->inode_size) / ext2->block_size;
    uint32_t off2 = (index - offset * (ext2->block_size / ext2->inode_size)) * ext2->inode_size;


    // Read the block
    // TODO: Cache?
    uint8_t *buf;
    int r = ext2_readBlock(ext2, inode_table_block + offset, &buf);
    if (r) return r;

    // Copy the inode structure
    memcpy(buf + off2, inode, ext2->inode_size);

    // Write the block back
    r = ext2_writeBlock(ext2, inode_table_block + offset, buf);
    kfree(buf);
    return r;
}

/**
 * @brief Convert a block on an inode to a block on disk
 * @param ext2 EXT2 filesystem
 * @param inode The inode to convert the block on
 * @param block The block number to read
 */
uint32_t ext2_convertInodeBlock(ext2_t *ext2, ext2_inode_t *inode, uint32_t block) {
    // EXT2 inodes have 12 direct blocks that can be used to address the inode
    if (block < EXT2_DIRECT_BLOCKS) {
        return inode->block_ptr[block];
    }
    
    size_t pointers_per_block = ext2->block_size / sizeof(uint32_t);

    // Otherwise, indirect blocks must be used. singly_indirect_block points to a block filled with addresses of inode blocks
    block -= EXT2_DIRECT_BLOCKS;
    if (block < pointers_per_block) {
        // Single indirect block can be used
        // Read the singly indirect block
        uint8_t *singly_indirect;

        int r = ext2_readBlock(ext2, inode->singly_indirect_block, &singly_indirect);
        if (r) {
            // What do we do..?
            LOG(ERR, "Error reading singly indirect block %d\n", inode->singly_indirect_block);
            return 0x0;
        }

        uint32_t blk = ((uint32_t*)singly_indirect)[block];
        kfree(singly_indirect);
        return blk;
    } else if (block < pointers_per_block + pointers_per_block * pointers_per_block) {
        // Doubly indirect

        // Read the doubly indirect block
        uint8_t *doubly_indirect;
        int r = ext2_readBlock(ext2, inode->doubly_indirect_block, &doubly_indirect);
        if (r) {
            LOG(ERR, "Error reading doubly indirect block %d\n", inode->doubly_indirect_block);
            return 0x0;
        }

        // Determine the index in the doubly indirect block
        uint32_t doubly_index = block / pointers_per_block;
        uint32_t singly_index = block % pointers_per_block;

        if (!((uint32_t *)doubly_indirect)[doubly_index]) {
            // Allocate a singly indirect block
            uint32_t blk = ext2_allocateBlock(ext2);
            ((uint32_t *)doubly_indirect)[doubly_index] = blk;

            // Write back the updated doubly indirect block
            ext2_writeBlock(ext2, inode->doubly_indirect_block, doubly_indirect);
        }

        // Read the singly indirect block
        uint8_t *singly_indirect;
        r = ext2_readBlock(ext2, ((uint32_t *)doubly_indirect)[doubly_index], &singly_indirect);
        if (r) {
            LOG(ERR, "Error reading singly indirect block %d\n", ((uint32_t *)doubly_indirect)[doubly_index]);
            kfree(doubly_indirect);
            return 0x0;
        }

        // Get the block number from the singly indirect block
        uint32_t blk = ((uint32_t *)singly_indirect)[singly_index];

        kfree(singly_indirect);
        kfree(doubly_indirect);
        return blk;
    }

    // ???
    LOG(ERR, "Block error: 0x%x\n", block);
    return 0x0;
}

/**
 * @brief Set a new inode block
 * @param ext2 Filesystem
 * @param inode The inode
 * @param iblock The block in the inode to set
 * @param block_num Disk block to set to
 */
int ext2_setInodeBlock(ext2_t *ext2, ext2_inode_t *inode, uint32_t iblock, uint32_t block_num) {
    if (iblock < EXT2_DIRECT_BLOCKS) {
        inode->block_ptr[iblock] = block_num;
        return 0;
    }


    size_t pointers_per_block = ext2->block_size / sizeof(uint32_t);

    // Otherwise, indirect blocks must be used. singly_indirect_block points to a block filled with addresses of inode blocks
    iblock -= EXT2_DIRECT_BLOCKS;
    if (iblock < pointers_per_block) {
        // Single indirect block can be used

        if (!inode->singly_indirect_block) {
            // Allocate a singly indirect block for the inode (and pray they flush it)
            uint32_t blk = ext2_allocateBlock(ext2);
            inode->singly_indirect_block = blk;
        }

        // Read the singly indirect block
        uint8_t *singly_indirect;

        int r = ext2_readBlock(ext2, inode->singly_indirect_block, &singly_indirect);
        if (r) {
            // What do we do..?
            LOG(ERR, "Error reading singly indirect block %d\n", inode->singly_indirect_block);
            return 0x0;
        }

        ((uint32_t*)singly_indirect)[iblock] = block_num;
        
        ext2_writeBlock(ext2, inode->singly_indirect_block, singly_indirect);
        kfree(singly_indirect);
        return 0;
    } else if (iblock < pointers_per_block + pointers_per_block * pointers_per_block) {
        // Doubly indirect

        if (!inode->doubly_indirect_block) {
            // Allocate a doubly indirect block for the inode
            uint32_t blk = ext2_allocateBlock(ext2);
            inode->doubly_indirect_block = blk;
        }

        // Read the doubly indirect block
        uint8_t *doubly_indirect;
        int r = ext2_readBlock(ext2, inode->doubly_indirect_block, &doubly_indirect);
        if (r) {
            LOG(ERR, "Error reading doubly indirect block %d\n", inode->doubly_indirect_block);
            return -EIO;
        }

        // Determine the index in the doubly indirect block
        uint32_t doubly_index = iblock / pointers_per_block;
        uint32_t singly_index = iblock % pointers_per_block;

        if (!((uint32_t *)doubly_indirect)[doubly_index]) {
            // Allocate a singly indirect block
            uint32_t blk = ext2_allocateBlock(ext2);
            ((uint32_t *)doubly_indirect)[doubly_index] = blk;

            // Write back the updated doubly indirect block
            ext2_writeBlock(ext2, inode->doubly_indirect_block, doubly_indirect);
        }

        // Read the singly indirect block
        uint8_t *singly_indirect;
        r = ext2_readBlock(ext2, ((uint32_t *)doubly_indirect)[doubly_index], &singly_indirect);
        if (r) {
            kfree(doubly_indirect);
            LOG(ERR, "Error reading singly indirect block %d\n", ((uint32_t *)doubly_indirect)[doubly_index]);
            return -EIO;
        }

        // Set the block number in the singly indirect block
        ((uint32_t *)singly_indirect)[singly_index] = block_num;

        // Write back the updated singly indirect block
        ext2_writeBlock(ext2, ((uint32_t *)doubly_indirect)[doubly_index], singly_indirect);

        kfree(singly_indirect);
        kfree(doubly_indirect);
        return 0;
    }

    // ???
    LOG(ERR, "Block error: 0x%x\n", iblock);
    return -EIO;
}

/**
 * @brief Read a block from an inode
 * @param ext2 EXT2 filesystem
 * @param inode The inode to convert the block on
 * @param block The block number to read
 * @param buffer Buffer location
 * @returns Error code
 */
int ext2_readInodeBlock(ext2_t *ext2, ext2_inode_t *inode, uint32_t block, uint8_t **buffer) {
    uint32_t blk = ext2_convertInodeBlock(ext2, inode, block);
    return ext2_readBlock(ext2, blk, buffer);
}


/**
 * @brief Write a block to an inode
 * @param ext2 EXT2 filesystem
 * @param inode The inode to convert the block on
 * @param block The block number to write
 * @param buffer Buffer location
 * @returns Error code
 */
int ext2_writeInodeBlock(ext2_t *ext2, ext2_inode_t *inode, uint32_t block, uint8_t *buffer) {
    uint32_t blk = ext2_convertInodeBlock(ext2, inode, block);
    return ext2_writeBlock(ext2, blk, buffer);
}


/**
 * @brief Allocate an inode in EXT2
 * @param ext2 EXT2 filesystem
 */
uint32_t ext2_allocateInode(ext2_t *ext2) {
    // For each BGD
    uint32_t inode = 0;
    uint8_t *iblock = NULL;
    uint32_t bgd = 0;
    
    for (; bgd < ext2->bgd_count; bgd++) {
        // Read the inode block array
        if (ext2_readBlock(ext2, ext2->bgds[bgd].inode_usage_bitmap, &iblock)) {
            return 0x0;
        }

        uint32_t off = 11;
        while (iblock[(off) >> 3] & (1 << (off % 8))) {
            off++;
            if (off >= ext2->inodes_per_group) {
                break;
            }
        }

        if (!(off >= ext2->inodes_per_group)) {
            // Offset is fine, return it
            inode = off + (bgd * ext2->inodes_per_group) + 1;
            break;
        }

        kfree(iblock);
    }

    if (!inode) return 0x0;

    // Set the bit in the BGD
    uint32_t bg_offset = (inode - (bgd * ext2->inodes_per_group) - 1);
    iblock[(bg_offset) >> 3] |= (1 << (bg_offset % 8));
    ext2_writeBlock(ext2, ext2->bgds[bgd].inode_usage_bitmap, iblock);

    // Lower BGD inode count
    ext2->bgds[bgd].unallocated_inodes--;
    ext2_flushBGDs(ext2);

    // Lower superblock inode count
    ext2->superblock.unallocated_inodes--;
    ext2_flushSuperblock(ext2);

    kfree(iblock);

    return inode;
}
