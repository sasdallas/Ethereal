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
#include <kernel/mem/alloc.h>
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
fs_drive_t *drive_mount(fs_node_t *node, int type) {
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

/**
 * @brief Register a new drive partition
 * @param drive The drive to register the partition on
 * @param node The node of the partition
 * @returns Partition object or NULL on failure
 * 
 * @note Partitions are automatically unmounted when the entire drive is unmounted
 */
fs_part_t *drive_mountPartition(fs_drive_t *drive, fs_node_t *node) {
    if (!drive->partition_list) drive->partition_list = list_create("drive partition list");

    // Allocate a new filesystem partition
    fs_part_t *part = kmalloc(sizeof(fs_part_t));
    memset(part, 0, sizeof(fs_part_t));

    part->parent = drive;
    part->node = node;
    part->part_number = part->parent->last_partition;

    // Create path
    snprintf(node->name, 256, "%sp%d", part->parent->name, part->part_number);

    // Mount
    if (vfs_mount(part->node, node->name) == NULL) {
        LOG(ERR, "Failed to mount new partition \"%s\"\n", node->name);
        kfree(part);
        return NULL;
    }

    // Increment last partition
    part->parent->last_partition++;

    // Return partition
    LOG(INFO, "Successfully mounted new partition \"%s\"\n", node->name);
    return part;
}

/**
 * @brief Find and get a drive by its path
 * @param path The path to search for (full path)
 * @returns The drive object or NULL if it could not be found
 */
fs_drive_t *drive_findPath(const char *path) {
    if (!drive_list) return NULL;

    foreach(node, drive_list) {
        fs_drive_t *drive = (fs_drive_t*)node->value;
        if (drive) {
            if (!strncmp(drive->name, path, 256)) {
                return drive;
            }
        }
    }

    return NULL;
}

/**
 * @brief Find and get a partition by its path
 * @param path The path to search for
 * @returns The drive object or NULL if it could not be found.
 */
fs_part_t *drive_findPathPartition(const char *path) {
    // !!!: This is kind of stupid and complex...



    // First get the drive
    int drive_path_index = path - strrchr(path, 'p');

    // Weird copy 
    char newpath[drive_path_index + 1];
    strncpy(newpath, path, drive_path_index);
    newpath[drive_path_index] = 0; 

    LOG(DEBUG, "Extracted path: %s\n", newpath);

    return NULL;
}

/**
 * @brief Unmount a drive
 * @param drive The drive to unmount
 */
void drive_unmount(fs_drive_t *drive) {
    if (!drive) return;

    // Unmount partitions
    foreach(partnode, drive->partition_list) {
        if (partnode->value) {
            // Manually do this to avoid drive_unmountPartition from unmounting and removing from list while we're going through list
            fs_part_t *part = (fs_part_t*)partnode->value;
            if (part->node) fs_close(part->node);
            kfree(part);
        }
    }

    // Close filesystem node
    if (drive->node) fs_close(drive->node);

    // Destroy partition list
    list_destroy(drive->partition_list, false); // will have been removed anyways

    // Remove from global drive list if needed
    if (drive_list) {
        node_t *node = list_find(drive_list, (void*)drive);
        if (node) {
            list_delete(drive_list, node);
        }
    }

    // TODO: Update indexes!!!!!!

    // Free drive
    kfree(drive);
}

/**
 * @brief Unmount a partition
 * @param part The partition to unmount
 */
void drive_unmountPartition(fs_part_t *part) {
    if (!part) return;

    // Close partition node
    if (part->node) fs_close(part->node);

    // Remove from global partition list
    if (part->parent && part->parent->partition_list) {
        node_t *node = list_find(part->parent->partition_list, (void*)part);
        if (node) {
            list_delete(part->parent->partition_list, node);
            kfree(node);    
        }
    }

    // TODO: Update last partition index??
    kfree(part);
}

