/**
 * @file drivers/fs/fat/fat.h
 * @brief FAT filesystem driver header
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _FAT_H
#define _FAT_H

/**** INCLUDES ****/
#include <stdint.h>
#include <kernel/fs/vfs.h>

/**** DEFINITIONS ****/

#define FAT_IDENTIFIER0     0xEB
#define FAT_IDENTIFIER1     0x3C
#define FAT_IDENTIFIER2     0x90

#define FAT_TYPE_UNKNOWN    0
#define FAT_TYPE_EXFAT      1
#define FAT_TYPE_FAT12      2
#define FAT_TYPE_FAT16      3
#define FAT_TYPE_FAT32      4

#define FAT_ATTR_READ_ONLY      0x01
#define FAT_ATTR_HIDDEN         0x02
#define FAT_ATTR_SYSTEM         0x04
#define FAT_ATTR_VOLUME_ID      0x08
#define FAT_ATTR_DIRECTORY      0x10
#define FAT_ATTR_ARCHIVE        0x20
#define FAT_ATTR_LFN            (FAT_ATTR_READ_ONLY | FAT_ATTR_HIDDEN | FAT_ATTR_SYSTEM | FAT_ATTR_VOLUME_ID)

#define FAT_CLUSTER_END         0xFFFFFFFF  // Driver-specific value (since FAT12/16 and FAT32 differ)
#define FAT_CLUSTER_CORRUPT     0xFFFFFFFE  // The cluster is bad



/**** TYPES ****/

typedef struct fat32_ebpb {
    uint32_t sectors_per_fat32; // Sectors per FAT (FAT32)
    uint16_t flags;             // Flags
    uint16_t version;           // FAT version number
    uint32_t rootdir_cluster;   // Cluster number of the root directory
    uint16_t fsinfo_sector;     // FSInfo structure
    uint16_t backup_sector;     // Backup boot sector
    uint8_t reserved[12];       // Reserved
    uint8_t dl;                 // Drive number
    uint8_t ntflags;            // Flags
    uint8_t ebpb_signature;     // Signature
    uint32_t serial_number;     // Serial number
    char label[11];             // Volume label
    char system_id[8];          // System identifier string
    uint8_t bootcode[420];      // Bootcode
    uint16_t signature;         // Signature
} __attribute__((packed)) fat32_ebpb_t;

typedef struct fat_ebpb {
    uint8_t dl;                 // Drive number
    uint8_t flags;              // Flags
    uint8_t ebpb_signature;     // Signature
    uint32_t serial_number;     // Serial number
    char label[11];             // Volume label
    char system_id[8];          // System identifier string
    uint8_t bootcode[448];      // Bootcode
    uint16_t signature;         // 0xAA55
} __attribute__((packed)) fat_ebpb_t;

typedef struct fat_bpb {
    uint8_t identifier[3];          // EB 3C 90
    char oemid[8];                  // OEM identifier
    uint16_t bytes_per_sector;      // Bytes per sector
    uint8_t sectors_per_cluster;    // Sectors per cluster
    uint16_t reserved_sectors;      // Reserved sector count
    uint8_t fats;                   // Total FATs
    uint16_t root_entries;          // Root directory entries
    uint16_t total_sectors;         // Total sectors
    uint8_t media_type;             // Media type
    uint16_t sectors_per_fat;       // Sectors per FAT
    uint16_t sectors_per_track;     // Sectors per track
    uint16_t head_count;            // Heads on the storage medium
    uint32_t hidden_sectors;        // Hidden sector count
    uint32_t large_sectors;         // Large sector count

    union {
        fat_ebpb_t fat_ebpb;
        fat32_ebpb_t fat32_ebpb;
    };
} __attribute__((packed)) fat_bpb_t;

typedef union fat_time {
    struct {
        uint8_t hour:5;             // Hour
        uint8_t minute:6;           // Minute
        uint8_t seconds:5;          // (multiply by two) seconds
    };

    uint16_t time;
} fat_time_t;

typedef union fat_date {
    struct {
        uint8_t year:7;             // Year
        uint8_t month:4;            // Month
        uint8_t day:5;              // Day
    };

    uint16_t date;
} fat_date_t;

typedef struct fat_entry {
    char filename[11];              // 8.3 filename
    uint8_t attributes;             // Attributes of the directory
    uint8_t reserved;               // Reserved
    uint8_t creation_time_hun;      // Hundreths of a second creation time
    uint16_t creation_time;         // Creation time
    uint16_t creation_date;         // Creation date
    uint16_t access_date;           // Last access date
    uint16_t cluster_high;          // Cluster number high
    uint16_t modification_time;     // Modification time
    uint16_t modification_date;     // Modification date
    uint16_t cluster_lo;            // Cluster number low
    uint32_t size;                  // Size in bytes
} __attribute__((packed)) fat_entry_t;

typedef struct fat_lfn {
    uint8_t order;                  // Order
    char name[10];                  // Name (2-byte)
    uint8_t attributes;             // FAT_ATTR_LFN
    uint8_t entry_type;             // Long entry tye
    uint8_t checksum;               // Checksum
    char name2[12];                 // Name (2-byte)
    uint16_t zero;                  // Always zero
    char name3[4];                  // Final parts of name
} __attribute__((packed)) fat_lfn_t;

typedef struct fat {
    fs_node_t *dev;                 // Device
    fat_bpb_t bpb;                  // BPB
    uint8_t type;                   // Type of FAT
} fat_t;

typedef struct fat_node {
    fat_entry_t *entry;             // Entry data
    fat_t *fat;                     // Global FAT filesystem
    
} fat_node_t;

/**** MACROS ****/

/* all stolen from OSDev wiki */
#define FAT_TOTAL_SECTORS(bpb) (bpb.total_sectors == 0 ? bpb.large_sectors : bpb.total_sectors)
#define FAT_TABLE_SECTORS(bpb) (bpb.sectors_per_fat == 0 ? bpb.fat32_ebpb.sectors_per_fat32 : bpb.sectors_per_fat)
#define FAT_ROOTDIR_SIZE(bpb) (((bpb.root_entries * 32) + (bpb.bytes_per_sector - 1)) / bpb.bytes_per_sector)
#define FAT_FIRST_DATA_SECTOR(bpb) (bpb.reserved_sectors + (bpb.fats * FAT_TABLE_SECTORS(bpb)) + FAT_ROOTDIR_SIZE(bpb))
#define FAT_FIRST_FAT_SECTOR(bpb) (bpb.reserved_sectors)
#define FAT_DATA_SECTORS(bpb) (FAT_TOTAL_SECTORS(bpb) - (bpb.reserved_sectors + (bpb.fats * FAT_TABLE_SECTORS(bpb)) + FAT_ROOTDIR_SIZE(bpb)))
#define FAT_TOTAL_CLUSTERS(bpb) (FAT_DATA_SECTORS(bpb) / bpb.sectors_per_cluster)
#define FAT_BYTES_PER_CLUSTER(bpb) (bpb.bytes_per_sector * bpb.sectors_per_cluster)

#endif