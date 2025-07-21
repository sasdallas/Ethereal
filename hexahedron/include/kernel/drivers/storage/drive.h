/**
 * @file hexahedron/include/kernel/drivers/storage/drive.h
 * @brief Hexahedron drive interface
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef DRIVERS_STORAGE_DRIVE_H
#define DRIVERS_STORAGE_DRIVE_H

/**** INCLUDES ****/
#include <kernel/fs/vfs.h>

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

/**** TYPES ****/

struct drive;

/**
 * @brief Read sectors method for a drive
 * @param drive The drive object to read sectors
 * @param lba The LBA to read
 * @param sectors The amount of sectors to read
 * @param buffer The buffer to read the sectors into
 * @returns Number of sectors read or error code
 */
typedef ssize_t (*drive_read_sectors_t)(struct drive*, uint64_t, size_t, uint8_t*);

/**
 * @brief Write sectors method for a drive
 * @param drive The drive object to write sectors
 * @param lba The LBA to write
 * @param sectors The amount of sectors to write
 * @param buffer The buffer to write the sectors using
 * @returns Number of sectors written or error code
 */
typedef ssize_t (*drive_write_sectors_t)(struct drive*, uint64_t, size_t, uint8_t*);



typedef struct drive {
    fs_node_t *node;                        // Mounted filesystem node for the drive
    int type;                               // Drive type
    size_t sectors;                         // Sectors of the drive
    size_t sector_size;                     // Sector size

    char *model;                            // Drive model
    char *serial;                           // Drive serial
    char *revision;                         // Drive revision
    char *vendor;                           // Drive vendor

    drive_read_sectors_t read_sectors;      // Read sectors method
    drive_write_sectors_t write_sectors;    // Write sectors method

    void *driver;                           // Driver-specific field
} drive_t;

/**** FUNCTIONS ****/

/**
 * @brief Create a new drive object
 * @param type The type of the drive being created
 * 
 * Please make sure you fill all fields possible out
 */
drive_t *drive_create(int type);

/**
 * @brief Mount the drive object
 * @param drive The drive object to mount
 * @returns 0 on success
 */
int drive_mount(drive_t *drive);

#endif