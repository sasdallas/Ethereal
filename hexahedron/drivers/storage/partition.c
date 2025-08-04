/**
 * @file hexahedron/drivers/storage/partition.c
 * @brief Partition driver
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/drivers/storage/drive.h>
#include <kernel/mem/alloc.h>
#include <kernel/debug.h>
#include <string.h>
#include <errno.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "STORAGE:PART", __VA_ARGS__)

/**
 * @brief Partition read method
 */
ssize_t partition_read(fs_node_t *node, off_t offset, size_t size, uint8_t *buffer) {
    partition_t *part = (partition_t*)node->dev;
    if (part->read) return part->read(part, offset, size, buffer);
    return -EINVAL;
}

/**
 * @brief Partition write method
 */
ssize_t partition_write(fs_node_t *node, off_t offset, size_t size, uint8_t *buffer) {
    partition_t *part = (partition_t*)node->dev;
    if (part->write) return part->write(part, offset, size, buffer);
    return -EINVAL;
}

/**
 * @brief Create a new partition on a drive
 * @param drive The drive to create the partition on
 * @param size Size of the partition
 */
partition_t *partition_create(struct drive *drive, size_t size) {
    partition_t *part = kzalloc(sizeof(partition_t));
    part->index = drive->last_part_index++;
    part->label = NULL;
    part->size = size;
    part->parent = drive;
    
    part->node = fs_node();
    strncpy(part->node->name, "partition", 256);
    part->node->mask = 0600;
    part->node->flags = VFS_BLOCKDEVICE;
    part->node->atime = part->node->mtime = part->node->ctime = now();
    part->node->read = partition_read;
    part->node->write = partition_write;
    part->node->dev = (void*)part;

    part->read = NULL;
    part->write = NULL;

    list_append(drive->partitions, part);

    return part;
}

/**
 * @brief Mount a partition
 * @param partition The partition to mount
 */
int partition_mount(partition_t *part) {
    // Create mount path
    char mount_path[256] = { 0 };
    snprintf(mount_path, 256, "%sp%d", part->parent->drivefs->name, part->index);
    vfs_mount(part->node, mount_path);
    return 0;
}