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
static int partition_read_range(devfs_node_t *node, page_range_t *range);
static int partition_write_range(devfs_node_t *node, page_range_t *range);

static devfs_ops_t partition_ops = {
    .open           = NULL,
    .close          = NULL,
    .read           = partition_read,
    .write          = partition_write,
    .ioctl          = NULL,
    .lseek          = NULL,
    .mmap           = NULL,
    .mmap_prepare   = NULL,
    .munmap         = NULL,
    .poll           = NULL,
    .poll_events    = NULL,
    .read_range     = partition_read_range,
    .write_range    = partition_write_range,
};

/**
 * @brief Partition read method
 */
static ssize_t partition_read(devfs_node_t *node, loff_t offset, size_t size, char *buffer) {
    partition_t *part = (partition_t*)node->priv;
    if (offset > (off_t)part->size) return 0;
    if (size + offset > part->size) size = part->size - offset;

    return part->parent->node->ops->read(part->parent->node, part->off + offset, size, buffer); // !!!
}

/**
 * @brief Partition write method
 */
static ssize_t partition_write(devfs_node_t *node, loff_t offset, size_t size, const char *buffer) {
    partition_t *part = (partition_t*)node->priv;
    if (offset > (off_t)part->size) return 0;
    if (size + offset > part->size) size = part->size - offset;

    return part->parent->node->ops->write(part->parent->node, part->off + offset, size, buffer); // !!!
}

/**
 * @brief Partition read range method
 */
static int partition_read_range(devfs_node_t *node, page_range_t *range) {
    partition_t *part = (partition_t*)node->priv;
    if (range->offset > (off_t)part->size) return 0;
    if (range->npages * PAGE_SIZE + range->offset > part->size) {
        // !!!!!! THIS IS A SEVERE BUG
        LOG(WARN, "Reading past bounds of partition!\n");
    }

    range->offset += part->off; // ??? !!!

    return part->parent->node->ops->read_range(part->parent->node, range); // !!!
}

/**
 * @brief Partition write range method
 */
static int partition_write_range(devfs_node_t *node, page_range_t *range) {
    partition_t *part = (partition_t*)node->priv;
    if (range->offset > (off_t)part->size) return 0;
    if (range->npages * PAGE_SIZE + range->offset > part->size) {
        // !!!!!! THIS IS A SEVERE BUG
        LOG(WARN, "Writing past bounds of partition, this is really bad!\n");
    }

    range->offset += part->off; // ??? !!!

    return part->parent->node->ops->write_range(part->parent->node, range); // !!!
}

/**
 * @brief Create a new partition on a drive
 * @param drive The drive to create the partition on
 * @param start_lba Start LBA of the partition
 * @param size Size of the partition
 * @param d Private data variable
 */
partition_t *partition_create(struct drive *drive, loff_t start_lba, size_t size_lba, void *d) {
    partition_t *part = kzalloc(sizeof(partition_t));
    part->index = drive->last_part_index++;
    part->label = NULL;
    part->size = size_lba * drive->sector_size;
    part->parent = drive;
    part->off = start_lba * drive->sector_size;;
    part->d = d;
    
    char mount_path[256];
    snprintf(mount_path, 256, "%sp%lu", part->parent->node->name, part->index);
    part->node = devfs_register(devfs_root, mount_path, VFS_BLOCKDEVICE, &partition_ops, part->parent->node->major, part->index, part);
    part->node->attr.size = part->size;

    part->lnode.value = part;
    list_append_node(drive->partitions, &part->lnode);

    return part;
}

/**
 * @brief Unmount and delete a partition
 * @param part The partition to delete
 */
void partition_delete(partition_t *part) {
    list_delete(part->parent->partitions, &part->lnode);
    devfs_unregister(part->node);

    kfree(part);
}

