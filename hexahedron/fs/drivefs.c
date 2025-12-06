/**
 * @file hexahedron/fs/drivefs.c
 * @brief Handles drive filesystem nodes in the kernel
 * 
 * This is responsible for registering storage drives into the VFS - it basically handles
 * partitions, physical drives, etc. and their naming (e.g. /device/sata0)
 * 
 * @todo Maybe replace indexes with bitmaps so we can allocate/free drive indexes?
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <kernel/fs/drivefs.h>
#include <kernel/mm/alloc.h>
#include <kernel/debug.h>

#include <string.h>

/* Available indexes */
static int index_ide_hd = 0;
static int index_cdrom = 0;
static int index_sata = 0;
static int index_scsi = 0;
static int index_scsi_cdrom = 0;
static int index_nvme = 0;
static int index_floppy = 0;
static int index_mmc = 0;
static int index_unknown = 0; // TODO: not this?

/* Macro to assist in getting the needed index */
#define GET_INDEX(type, out)    {\
                                    if (type == DRIVE_TYPE_IDE_HD) out = &index_ide_hd;\
                                    else if (type == DRIVE_TYPE_CDROM) out = &index_cdrom;\
                                    else if (type == DRIVE_TYPE_SATA) out = &index_sata;\
                                    else if (type == DRIVE_TYPE_SCSI) out = &index_scsi;\
                                    else if (type == DRIVE_TYPE_SCSI_CDROM) out = &index_scsi_cdrom;\
                                    else if (type == DRIVE_TYPE_NVME) out = &index_nvme;\
                                    else if (type == DRIVE_TYPE_FLOPPY) out = &index_floppy;\
                                    else if (type == DRIVE_TYPE_MMC) out = &index_mmc;\
                                    else out = &index_unknown;\
                                }

/* List of drives */
list_t *drive_list = NULL; // Auto-created on first drive mount

/* Log method */
#define LOG(status, ...) dprintf_module(status, "FS:DRIVE", __VA_ARGS__)




/**
 * @brief Register a new drive for Hexahedron
 * @param node The filesystem node of the drive
 * @param type The type of the drive
 * @returns Drive object or NULL on failure
 */
fs_drive_t *drive_mountNode(fs_node_t *node, int type) {
    if (!node) return NULL;

    // Create the list if its empty
    if (drive_list == NULL) drive_list = list_create("drive list");

    // Get the index
    int *index; // TODO: find
    GET_INDEX(type, index);

    // Allocate a new drive object
    fs_drive_t *drive = kmalloc(sizeof(fs_drive_t));
    memset(drive, 0, sizeof(fs_drive_t));

    // Assign drive variables
    drive->node = node;
    drive->type = type;

    // Construct drive path
    switch (type) {
        case DRIVE_TYPE_IDE_HD:
            snprintf(drive->name, 256, "/device/" DRIVE_NAME_IDE_HD "%i", *index);
            snprintf(drive->node->name, 256, DRIVE_NAME_IDE_HD "%i", *index);
            break;
        case DRIVE_TYPE_CDROM:
            snprintf(drive->name, 256, "/device/" DRIVE_NAME_CDROM "%i", *index);
            snprintf(drive->node->name, 256, DRIVE_NAME_CDROM "%i", *index);
            break;
        case DRIVE_TYPE_SATA:
            snprintf(drive->name, 256, "/device/" DRIVE_NAME_SATA "%i", *index);
            snprintf(drive->node->name, 256, DRIVE_NAME_SATA "%i", *index);
            break;
        case DRIVE_TYPE_SCSI:
            snprintf(drive->name, 256, "/device/" DRIVE_NAME_SCSI "%i", *index);
            snprintf(drive->node->name, 256, DRIVE_NAME_SCSI "%i", *index);
            break;
        case DRIVE_TYPE_SCSI_CDROM:
            snprintf(drive->name, 256, "/device/" DRIVE_NAME_SCSI_CDROM "%i", *index);
            snprintf(drive->node->name, 256, DRIVE_NAME_SCSI_CDROM "%i", *index);
            break;
        case DRIVE_TYPE_NVME:
            snprintf(drive->name, 256, "/device/" DRIVE_NAME_NVME "%i", *index);
            snprintf(drive->node->name, 256, DRIVE_NAME_NVME "%i", *index);
            break;
        case DRIVE_TYPE_FLOPPY:
            snprintf(drive->name, 256, "/device/" DRIVE_NAME_FLOPPY "%i", *index);
            snprintf(drive->node->name, 256, DRIVE_NAME_FLOPPY "%i", *index);
            break;
        case DRIVE_TYPE_MMC:
            snprintf(drive->name, 256, "/device/" DRIVE_NAME_MMC "%i", *index);
            snprintf(drive->node->name, 256, DRIVE_NAME_MMC "%i", *index);
            break;
        default:
            snprintf(drive->name, 256, "/device/" DRIVE_NAME_UNKNOWN "%i", *index);
            snprintf(drive->node->name, 256, DRIVE_NAME_UNKNOWN "%i", *index);
            break;
    }

    // Mount drive
    if (vfs_mount(drive->node, drive->name) == NULL) {
        LOG(ERR, "Error mounting drive \"%s\" - vfs_mount returned NULL\n", drive->name);
        kfree(drive);
        return NULL;
    } 

    // Append to list
    list_append(drive_list, (void*)drive);

    // Increment index
    *index = *index + 1;

    // Done!
    LOG(INFO, "Successfully mounted new drive \"%s\"\n", drive->name);
    return drive;
}