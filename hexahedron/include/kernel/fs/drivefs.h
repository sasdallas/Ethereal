/**
 * @file hexahedron/include/kernel/fs/drivefs.h
 * @brief Drive filesystem node handler
 * 
 * @see drivefs.c for explanation on what this does
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_FS_DRIVEFS_H
#define KERNEL_FS_DRIVEFS_H

/**** INCLUDES ****/
#include <stdint.h>
#include <kernel/fs/vfs.h>
#include <structs/list.h>

/**** DEFINITIONS ****/

// Drive types
#define DRIVE_TYPE_IDE_HD       1   // IDE harddrive
#define DRIVE_TYPE_CDROM        2   // CD-ROM (use DRIVE_TYPE_SCSI_CDROM for SCSI CDROMs)
#define DRIVE_TYPE_SATA         3   // SATA drive
#define DRIVE_TYPE_SCSI         4   // SCSI drive
#define DRIVE_TYPE_SCSI_CDROM   5   // SCSI CD-rom
#define DRIVE_TYPE_NVME         6   // NVMe drive
#define DRIVE_TYPE_FLOPPY       7   // Floppy drive
#define DRIVE_TYPE_MMC          8   // MMC drive

// Drive name prefixes
#define DRIVE_NAME_IDE_HD       "idehd"
#define DRIVE_NAME_CDROM        "cdrom"
#define DRIVE_NAME_SATA         "sata"
#define DRIVE_NAME_SCSI         "scsi"
#define DRIVE_NAME_SCSI_CDROM   "scsicd"
#define DRIVE_NAME_NVME         "nvme"
#define DRIVE_NAME_FLOPPY       "floppy"
#define DRIVE_NAME_MMC          "mmc"
#define DRIVE_NAME_UNKNOWN      "unknown"

/**** TYPES ****/

/**
 * @brief Filesystem drive object
 */
typedef struct fs_drive {
    fs_node_t *node;            // Filesystem node of the actual drive
    int type;                   // Type of the drive (e.g. DRIVE_TYPE_SATA)
    char name[256];             // Full filesystem name (e.g. "/device/cdrom0")
    int last_partition;         // Last partition given
    list_t *partition_list;     // List of drive partitions
} fs_drive_t;

/**
 * @brief Filesystem partition object
 */
typedef struct fs_part {
    fs_node_t *node;        // Filesystem node of the partition
    fs_drive_t *parent;     // Parent drive
    int part_number;        // Partition number
} fs_part_t;

/**** FUNCTIONS ****/

/**
 * @brief Register a new drive for Hexahedron
 * @param node The filesystem node of the drive
 * @param type The type of the drive
 * @returns Drive object or NULL on failure
 */
fs_drive_t *drive_mount(fs_node_t *node, int type);

/**
 * @brief Register a new drive partition
 * @param drive The drive to register the partition on
 * @param node The node of the partition
 * @returns Partition object or NULL on failure
 * 
 * @note Partitions are automatically unmounted when the entire drive is unmounted
 */
fs_part_t *drive_mountPartition(fs_drive_t *drive, fs_node_t *node);

/**
 * @brief Find and get a drive by its path
 * @param path The path to search for (full path)
 * @returns The drive object or NULL if it could not be found
 */
fs_drive_t *drive_findPath(const char *path);

/**
 * @brief Find and get a partition by its path
 * @param path The path to search for (full path)
 * @returns The drive object or NULL if it could not be found.
 */
fs_part_t *drive_findPathPartition(const char *path);

/**
 * @brief Unmount a drive
 * @param drive The drive to unmount
 */
void drive_unmount(fs_drive_t *drive);

/**
 * @brief Unmount a partition
 * @param part The partition to unmount
 */
void drive_unmountPartition(fs_part_t *part);


#endif