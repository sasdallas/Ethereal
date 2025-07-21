/**
 * @file hexahedron/include/kernel/fs/drivefs.h
 * @brief Drive filesystem node handler
 * 
 * @see drivefs.c for explanation on what this does
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
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
#include <kernel/drivers/storage/drive.h>

/**** DEFINITIONS ****/

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

/**** FUNCTIONS ****/

/**
 * @brief Register a new drive for Hexahedron
 * @param node The filesystem node of the drive
 * @param type The type of the drive
 * @returns Drive object or NULL on failure
 */
fs_drive_t *drive_mountNode(fs_node_t *node, int type);


#endif