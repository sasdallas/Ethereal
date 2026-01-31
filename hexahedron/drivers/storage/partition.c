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
#include <kernel/mm/alloc.h>
#include <kernel/debug.h>
#include <string.h>
#include <errno.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "STORAGE:PART", __VA_ARGS__)

static ssize_t partition_write(devfs_node_t *node, loff_t offset, size_t size, const char *buffer);
static ssize_t partition_read(devfs_node_t *node, loff_t offset, size_t size, char *buffer);

static devfs_ops_t partition_ops = {
    .open           = NULL,
    .close          = NULL,
    .read           = partition_read,
    .write          = partition_write,
    .ioctl          = NULL,
    .mmap           = NULL,
    .mmap_prepare   = NULL,
    .munmap         = NULL,
    .poll           = NULL,
    .poll_events    = NULL
};

/**
 * @brief Partition read method
 */
static ssize_t partition_read(devfs_node_t *node, loff_t offset, size_t size, char *buffer) {
    partition_t *part = (partition_t*)node->priv;
    if (part->ops->read) return part->ops->read(part, offset, size, buffer);
    return -ENODEV;
}

/**
 * @brief Partition write method
 */
static ssize_t partition_write(devfs_node_t *node, loff_t offset, size_t size, const char *buffer) {
    partition_t *part = (partition_t*)node->priv;
    if (part->ops->write) return part->ops->write(part, offset, size, buffer);
    return -ENODEV;
}

/**
 * @brief Create a new partition on a drive
 * @param drive The drive to create the partition on
 * @param size Size of the partition
 * @param ops Partition operations
 * @param d Private data variable
 */
partition_t *partition_create(struct drive *drive, size_t size, partition_ops_t *ops, void *d) {
    partition_t *part = kzalloc(sizeof(partition_t));
    part->index = drive->last_part_index++;
    part->label = NULL;
    part->size = size;
    part->parent = drive;
    part->ops = ops;
    part->d = d;
    
    char mount_path[256];
    snprintf(mount_path, 256, "%sp%lu", part->parent->node->name, part->index);
    part->node = devfs_register(devfs_root, mount_path, VFS_BLOCKDEVICE, &partition_ops, part->parent->node->major, part->index, part);

    list_append(drive->partitions, part);

    return part;
}