/**
 * @file drivers/fs/ext2/ext2_structures.h
 * @brief Contains the EXT2 structures (inode, superblock, etc.)
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _EXT2_STRUCTURES_H
#define _EXT2_STRUCTURES_H

/**** INCLUDES ****/
#include <stdint.h>
#include <kernel/misc/util.h>
#include <sys/types.h>

/**** TYPES ****/

typedef struct _ext2_sb {
    uint32_t inode_count;
    uint32_t block_count;
    uint32_t superuser_rsvd_blocks;
    uint32_t unallocated_blocks;
    uint32_t unallocated_inodes;
    uint32_t start_block_number;
    uint32_t block_size;
    uint32_t fragment_size;
    uint32_t bg_block_count;
    uint32_t bg_frag_count;
    uint32_t bg_ino_count;
    uint32_t mtime;
    uint32_t wtime;
    uint16_t mounts_since_fsck;
    uint16_t mounts_allowed_fsck;
    uint16_t signature;
    uint16_t state;
    uint16_t error_action;
    uint16_t ver_minor;
    uint32_t last_check_time;
    uint32_t check_interval;
    uint32_t osid;
    uint32_t ver_major;
    uint16_t rsvd_uid;
    uint16_t rsvd_gid;

    struct {
        uint32_t first_nonreserved;
        uint16_t inode_struct_size;
        uint16_t superblock_bg;
        uint32_t features;
        uint32_t features_required;
        uint32_t features_write;
        char fsid[16];
        char volname[16];
        char volpath[64];
        uint32_t compression_algorithms;
        uint8_t prealloc_file;
        uint8_t prealloc_dir;
        uint16_t unused;
        char journalid[16];
        uint32_t journal_inode;
        uint32_t journal_device;
        uint32_t orphan_inode_list;
        char pad[785];
    } extended;
} __attribute__((packed)) ext2_sb_t;

STATIC_ASSERT(sizeof(ext2_sb_t) == 1024);
STATIC_ASSERT(__builtin_offsetof(ext2_sb_t, extended) == 84);

typedef struct _ext2_bgd {
    uint32_t block_usage_bitmap;
    uint32_t inode_usage_bitmap;
    uint32_t inode_table;
    uint16_t free_blocks;
    uint16_t free_inodes;
    uint16_t directories;
    char pad[14];
} __attribute__((packed)) ext2_bgd_t;

STATIC_ASSERT(sizeof(ext2_bgd_t) == 32);

typedef struct _ext2_inode {
    uint16_t type_permissions;
    uint16_t uid;
    uint32_t size_lower;
    uint32_t atime;
    uint32_t ctime;
    uint32_t mtime;
    uint32_t dtime;
    uint16_t gid;
    uint16_t nlink;
    uint32_t disk_sectors;
    uint32_t flags;
    uint32_t os_specific;
    uint32_t dbp[12];
    uint32_t sibp;
    uint32_t dibp;
    uint32_t tibp;
    uint32_t gen;
    uint32_t acl; // applies to ext2 version >= 1
    uint32_t size_upper; // applies to ext2 version >= 1
    uint32_t frag_addr;
    char os_specific2[12];
} __attribute__((packed, aligned(4))) ext2_inode_t;

// aligned(4) forces the compiler to not emit a address-of-packed-member error

STATIC_ASSERT(sizeof(ext2_inode_t) == 128);

typedef struct _ext2_dirent {
    uint32_t inode;
    uint16_t size;
    uint8_t name_length; // lsb
    uint8_t type;
    char name[];
} __attribute__((packed)) ext2_dirent_t;

/**** DEFINITIONS ****/

#define EXT2_SB_OFFSET      1024
#define EXT2_SB_SIGNATURE   0xEF53

#define EXT2_STATE_CLEAN    1
#define EXT2_STATE_ERROR    2

#define EXT2_HANDLE_IGNORE  0
#define EXT2_HANDLE_REMOUNT 1
#define EXT2_HANDLE_PANIC   2

#define EXT2_ROOT_INODE 2

#define EXT2_DIRECT_BLOCK_POINTERS      12

/* features */
#define EXT2_FEATURE_PREALLOCATE        0x0001
#define EXT2_FEATURE_AFS                0x0002
#define EXT2_FEATURE_JOURNAL            0x0004
#define EXT2_FEATURE_INO_EXTENDED       0x0008
#define EXT2_FEATURE_RESIZE             0x0010
#define EXT2_FEATURE_HASH_INDEX         0x0020
#define EXT2_REQUIRES_COMPRESSION       0x0001
#define EXT2_REQUIRES_DIRECTORY_TYPE    0x0002
#define EXT2_REQUIRES_JOURNAL_REPLAY    0x0004
#define EXT2_REQUIRES_JOURNAL_DEVICE    0x0008
#define EXT2_WRITE_SPARSE               0x0001
#define EXT2_WRITE_64BIT                0x0002
#define EXT2_WRITE_BTREE                0x0004

/* inode permissions */
#define EXT2_INODE_PERM_XOTH            0x0001
#define EXT2_INODE_PERM_WOTH            0x0002
#define EXT2_INODE_PERM_ROTH            0x0004
#define EXT2_INODE_PERM_XGRP            0x0008
#define EXT2_INODE_PERM_WGRP            0x0010
#define EXT2_INODE_PERM_RGRP            0x0020
#define EXT2_INODE_PERM_XUSR            0x0040
#define EXT2_INODE_PERM_WUSR            0x0080
#define EXT2_INODE_PERM_RUSR            0x0100
#define EXT2_INODE_PERM_SVTX            0x0200
#define EXT2_INODE_PERM_SGID            0x0400
#define EXT2_INODE_PERM_SUID            0x0800

/* inode types */
#define EXT2_INODE_TYPE_FIFO            0x1000
#define EXT2_INODE_TYPE_CHARDEV         0x2000
#define EXT2_INODE_TYPE_DIR             0x4000
#define EXT2_INODE_TYPE_BLKDEV          0x6000
#define EXT2_INODE_TYPE_FILE            0x8000
#define EXT2_INODE_TYPE_SYMLINK         0xA000
#define EXT2_INODE_TYPE_SOCKET          0xC000

/* inode flags */
#define EXT2_INODE_FLAG_SYNC_UPDATES    0x00008
#define EXT2_INODE_FLAG_IMMUTABLE       0x00010
#define EXT2_INODE_FLAG_APPEND_ONLY     0x00020
#define EXT2_INODE_FLAG_NO_DUMP         0x00040
#define EXT2_INODE_FLAG_NO_ATIME        0x00080
#define EXT2_INODE_FLAG_HASH_DIR        0x10000
#define EXT2_INODE_FLAG_AFS_DIR         0x20000
#define EXT2_INODE_FLAG_JOURNAL         0x4000

#endif
