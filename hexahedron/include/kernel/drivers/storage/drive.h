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
#include <kernel/fs/devfs.h>
#include <kernel/drivers/storage/partition.h>
#include <kernel/drivers/storage/mbr.h>
#include <ethereal/drive.h>
#include <kernel/mm/cache.h>

/**** DEFINITIONS ****/

/**** TYPES ****/

struct drive;

typedef struct drive_ops {
    ssize_t (*read_sectors)(struct drive*, uint64_t, size_t, uint8_t*);
    ssize_t (*write_sectors)(struct drive*, uint64_t, size_t, uint8_t*);
    ssize_t (*read_range)(struct drive*, page_range_t*);
    ssize_t (*write_range)(struct drive*,page_range_t*);
    int (*ioctl)(struct drive*, int, void *);
    int (*flush)(struct drive*);
} drive_ops_t;

typedef struct drive {
    devfs_node_t *node;                     // Device filesystem node
    unsigned char type;                     // Drive type
    size_t sectors;                         // Sectors of the drive
    size_t sector_size;                     // Sector size

    char *model;                            // Drive model
    char *serial;                           // Drive serial
    char *revision;                         // Drive revision
    char *vendor;                           // Drive vendor

    drive_ops_t *ops;

    list_t *partitions;                     // Partitions
    unsigned long last_part_index;          // Last partition index

    void *driver;                           // Driver-specific field
} drive_t;

/**** FUNCTIONS ****/

/**
 * @brief Create a new drive object
 * @param type The type of the drive being created
 * @param ops Drive operations
 * 
 * Please make sure you fill all fields possible out
 */
drive_t *drive_create(int type, drive_ops_t *ops);

/**
 * @brief Mount the drive object
 * @param drive The drive object to mount
 * @returns 0 on success
 */
int drive_mount(drive_t *drive);

#endif