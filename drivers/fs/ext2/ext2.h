/**
 * @file drivers/fs/ext2/ext2.h
 * @brief EXT2 filesystem definitions
 * 
 * @ref https://www.nongnu.org/ext2-doc/ext2.html
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef EXT2_H
#define EXT2_H

/**** INCLUDES ****/
#include <stdint.h>
#include <kernel/fs/vfs.h>
#include <kernel/debug.h>

/**** DEFINITIONS ****/

#define EXT2_SIGNATURE                      0xEF53

/* Extended superblock field defaults */
/* Technically, versions < 1.0 should have these there but just in case */
#define EXT2_DEFAULT_INODE_SIZE             128

/* Inode flags */
#define EXT2_INODE_TYPE_FIFO                0x1000
#define EXT2_INODE_TYPE_CHARDEV             0x2000
#define EXT2_INODE_TYPE_DIRECTORY           0x4000
#define EXT2_INODE_TYPE_BLKDEV              0x6000
#define EXT2_INODE_TYPE_FILE                0x8000
#define EXT2_INODE_TYPE_SYMLINK             0xA000
#define EXT2_INODE_TYPE_SOCKET              0xC000

/* Inode permissions */
#define EXT2_INODE_IXOTH                    0x0001
#define EXT2_INODE_IWOTH                    0x0002
#define EXT2_INODE_IROTH                    0x0004
#define EXT2_INODE_IXGRP                    0x0008
#define EXT2_INODE_IWGRP                    0x0010
#define EXT2_INODE_IRGRP                    0x0020
#define EXT2_INODE_IXUSR                    0x0040
#define EXT2_INODE_IWUSR                    0x0080
#define EXT2_INODE_IRUSR                    0x0100
#define EXT2_INODE_STICKY                   0x0200
#define EXT2_INODE_ISGID                    0x0400
#define EXT2_INODE_ISUID                    0x0800

/* Blocks */
#define EXT2_DIRECT_BLOCKS                  12
#define XET2_

/**** TYPES ****/

typedef struct ext2_superblock_extended {
    uint32_t first_inode;                   // First non-reserved inode in filesystem
    uint16_t inode_size;                    // Size of inode structure in bytes
    uint16_t superblock_bg;                 // Block group of superblock
    uint32_t optional_features;             // Optional features
    uint32_t required_features;             // Required features
    uint32_t ro_features;                   // R/O features

    // STRINGS
    char filesystem_id[16];                 // Filesystem ID
    char volume_name[16];                   // Volume name
    char last_mount_path[64];               // Last mount path

    uint32_t compression_algorithms;        // Compression algorithms
    uint8_t preallocate_file_count;         // Number of blocks to preallocate for files
    uint8_t preallocate_directory_count;    // Number of blocks to preallocate for directories
    uint16_t unused;                        // Unused

    // JOURNALING
    char journal_uuid[16];                  // Journal superblock UUID
    uint32_t journal_inode;                 // Inode number of journal file
    uint32_t journal_last_orphan;           // First inode in list of inodes to delete

    // HASHING
    uint32_t hash_seed[4];                  // Hash algorithm seeds for directory indexiing
    uint8_t def_hash_version;               // Default hash version

    uint32_t default_mount_options;         // Default mount options
    uint32_t first_meta_bg;                 // First meta blockgroup

} ext2_superblock_extended_t;

typedef struct ext2_superblock {
    uint32_t inode_count;                   // Inode count
    uint32_t block_count;                   // Block count
    uint32_t superuser_reserved_blocks;     // Superuser reserved blocks
    uint32_t unallocated_blocks;            // Unallocated blocks
    uint32_t unallocated_inodes;            // Unallocated inodes
    uint32_t starting_block;                // Starting block (and block of the superblock)
    uint32_t block_size_unshifted;          // log2 (block size) - 10
    uint32_t fragment_size_unshifted;       // log2 (fragment size) - 10
    uint32_t bg_block_count;                // Block group block count
    uint32_t bg_fragment_count;             // Block group fragment count
    uint32_t bg_inode_count;                // Block group inode count;
    uint32_t last_mount_time;               // Last mount time
    uint32_t last_written_time;             // Last written time
    uint16_t mounts_since_check;            // Mounts since last fsck
    uint16_t mounts_allowed;                // Allowed mounts before consistency check
    uint16_t signature;                     // 0xEF53
    uint16_t fs_state;                      // Filesystem state
    uint16_t error_handling;                // Error handling method
    uint16_t version_minor;                 // Minor version
    uint32_t last_consistency_check;        // Last consistency check
    uint32_t interval_until_check;          // Interval until forced consistency check
    uint32_t creation_os;                   // Creation OS ID
    uint32_t version_major;                 // Major portion of version
    uint16_t reserved_block_uid;            // UID of user that can use reserved blocks
    uint16_t reserved_block_gid;            // GID of group that can use reserved blocks

    ext2_superblock_extended_t extended;    // Extended superblock
} __attribute__((packed)) ext2_superblock_t;

typedef struct ext2_bgd {
    uint32_t block_usage_bitmap;            // Block number of block usage bitmap
    uint32_t inode_usage_bitmap;            // Block number of inode usage bitmap
    uint32_t inode_table;                   // Inode table
    uint16_t unallocated_blocks;            // Unallocated blocks
    uint16_t unallocated_inodes;            // Unallocated inodes
    uint16_t directory_count;               // Directories
    uint16_t pad;                           // Padding
    uint8_t reserved[12];                   // Reserved
} __attribute__((packed)) ext2_bgd_t;

typedef struct ext2_inode {
    uint16_t type;                          // Type + permissions
    uint16_t uid;                           // User ID
    uint32_t size_low;                      // Lower bits of size
    uint32_t atime;                         // Access time
    uint32_t ctime;                         // Creation time
    uint32_t mtime;                         // Modification time
    uint32_t dtime;                         // Deletion time
    uint16_t gid;                           // Group ID
    uint16_t nlink;                         // Hard link count
    uint32_t disk_sectors_used;             // Disk sectors in use
    uint32_t flags;                         // Flags
    uint32_t os_specific;                   // OS-specific

    // BLOCKS
    uint32_t block_ptr[12];                 // Direct block pointers
    uint32_t singly_indirect_block;         // Singly indirect block pointer
    uint32_t doubly_indirect_block;         // Doubly indirect block pointer
    uint32_t triply_indirect_block;         // Triply indirect block pointer

    // EXTENDED FIELDS
    uint32_t generation_number;             // Generation number
    uint32_t extended_attribute_block;      // (version >= 1) Extended attribute block
    uint32_t size_upper;                    // (version >= 1) Upper 32 bits of size
    
    uint32_t block_address;                 // Block address of fragment
    uint8_t os_specific2[12];               // OS-specific
} __attribute__((packed)) ext2_inode_t;

typedef struct ext2_dirent {
    uint32_t inode;                         // Inode
    uint16_t entry_size;                    // Total size of the entry
    uint8_t name_length;                    // Name length
    uint8_t type_indicator;                 // Type indicator/MSB of name length
    char name[];
} __attribute__((packed)) ext2_dirent_t;

typedef struct ext2 {
    fs_node_t *drive;                       // Drive

    // GENERIC EXT2 INFORMATION
    uint32_t block_size;                    // Block size from superblock
    uint32_t inode_size;                    // Inode size
    uint32_t inodes_per_group;              // Inodes per group
    uint16_t bgd_offset;                    // BGD offset

    // BGD
    size_t bgd_count;                       // BGD count
    size_t bgd_blocks;                      // BGD blocks
    ext2_bgd_t *bgds;                       // Array of BGDs

    // SUPERBLOCK
    ext2_superblock_t superblock;           // Superblock object
} ext2_t;

/**** MACROS ****/

#define EXT2_EXTENDED(ext2)             ((ext2)->superblock.version_major >= 1)
#define EXT2_BGD(bgnum)                 (&(ext2->bgds[(bgnum)]))

/* Log method */
#define LOG(status, ...) dprintf_module(status, "DRIVER:EXT2", __VA_ARGS__)


/**** FUNCTIONS ****/

/**
 * @brief Read a block from an EXT2 filesystem
 * @param ext2 The filesystem to read
 * @param block The block number to read
 * @param buffer Buffer location
 * @returns Error code
 */
int ext2_readBlock(ext2_t *ext2, uint32_t block, uint8_t **buffer);

/**
 * @brief Write a block to an EXT2 filesystem
 * @param ext2 The filesystem to write
 * @param block The block number to write
 * @param buffer Buffer location
 * @returns Error code
 */
int ext2_writeBlock(ext2_t *ext2, uint32_t block, uint8_t *buffer);

/**
 * @brief Read the inode metadata from an EXT2 filesystem
 * @param ext2 The filesystem to read
 * @param inode The inode number to read
 * @param buffer The buffer location
 * @returns Error code
 */
int ext2_readInode(ext2_t *ext2, uint32_t inode, uint8_t **buffer);

/**
 * @brief Write an inode structure
 * @param ext2 EXT2 filesystem
 * @param inode The inode structure to write
 * @param inode_number The inode number
 */
int ext2_writeInode(ext2_t *ext2, ext2_inode_t *inode, uint32_t inode_number);

/**
 * @brief Write the superblock after changes were made
 * @param ext2 EXT2 filesystem
 */
void ext2_flushSuperblock(ext2_t *ext2);

/**
 * @brief Flush all block group descriptors
 * @param ext2 EXT2 filesystem
 */
void ext2_flushBGDs(ext2_t *ext2);

/**
 * @brief Convert inode to VFS node
 * @param ext2 EXT2 filesystem
 * @param inode The inode to convert
 * @param inode_number The number of the inode
 * @param name The name of the file
 */
fs_node_t *ext2_inodeToNode(ext2_t *ext2, ext2_inode_t *inode, uint32_t inode_number, char *name);

/**
 * @brief Convert a block on an inode to a block on disk
 * @param ext2 EXT2 filesystem
 * @param inode The inode to convert the block on
 * @param block The block number to read
 */
uint32_t ext2_convertInodeBlock(ext2_t *ext2, ext2_inode_t *inode, uint32_t block);

/**
 * @brief Read a block from an inode
 * @param ext2 EXT2 filesystem
 * @param inode The inode to convert the block on
 * @param block The block number to read
 * @param buffer Buffer location
 * @returns Error code
 */
int ext2_readInodeBlock(ext2_t *ext2, ext2_inode_t *inode, uint32_t block, uint8_t **buffer);

/**
 * @brief Allocate an inode in EXT2
 * @param ext2 EXT2 filesystem
 */
uint32_t ext2_allocateInode(ext2_t *ext2);

/**
 * @brief Allocate a new block in an EXT2 filesystem
 * @param ext2 The filesystem to allocate a block in
 */
uint32_t ext2_allocateBlock(ext2_t *ext2);

/**
 * @brief Write a block to an inode
 * @param ext2 EXT2 filesystem
 * @param inode The inode to convert the block on
 * @param block The block number to write
 * @param buffer Buffer location
 * @returns Error code
 */
int ext2_writeInodeBlock(ext2_t *ext2, ext2_inode_t *inode, uint32_t block, uint8_t *buffer);

/**
 * @brief Set a new inode block
 * @param ext2 Filesystem
 * @param inode The inode
 * @param iblock The block in the inode to set
 * @param block_num Disk block to set to
 */
int ext2_setInodeBlock(ext2_t *ext2, ext2_inode_t *inode, uint32_t iblock, uint32_t block_num);

#endif