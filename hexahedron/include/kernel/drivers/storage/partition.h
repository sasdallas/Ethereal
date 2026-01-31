/**
 * @file hexahedron/include/kernel/drivers/storage/partition.h
 * @brief Partition storage driver
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef DRIVERS_STORAGE_PARTITION_H
#define DRIVERS_STORAGE_PARTITION_H

/**** INCLUDES ****/
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include <kernel/fs/vfs.h>

/**** TYPES ****/

struct drive;
struct partition;

typedef struct partition_ops {
    ssize_t (*read)(struct partition *part, loff_t off, size_t size, char *buffer);
    ssize_t (*write)(struct partition *part, loff_t off, size_t size, const char *buffer);
} partition_ops_t;

typedef struct partition {
    struct drive *parent;               // Parent drive
    size_t size;                        // Size of partition
    char *label;                        // Optional partition label
    unsigned long index;                // Last partition index
    devfs_node_t *node;                 // Device filesystem node

    partition_ops_t *ops;               // Partition operations
    void *d;                            // Driver-specific
} partition_t;


/**** FUNCTIONS ****/

/**
 * @brief Create a new partition on a drive
 * @param drive The drive to create the partition on
 * @param size Size of the partition
 * @param ops Partition operations
 * @param d Private data variable
 */
partition_t *partition_create(struct drive *drive, size_t size, partition_ops_t *ops, void *d);

#endif