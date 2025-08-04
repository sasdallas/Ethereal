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

/**
 * @brief Partition read method
 * @param partition The partition to read
 * @param off The offset to read at
 * @param size The size to read at
 * @param buffer The buffer to read into
 */
typedef ssize_t (*partition_read_t)(struct partition *part, off_t off, size_t size, uint8_t *buffer);

/**
 * @brief Partition write method
 * @param partition The partition to write
 * @param off The offset to write to
 * @param size The size to write to
 * @param buffer The buffer to write from
 */
typedef ssize_t (*partition_write_t)(struct partition *part, off_t off, size_t size, uint8_t *buffer);

typedef struct partition {
    struct drive *parent;               // Parent drive
    size_t size;                        // Size of partition
    char *label;                        // Optional partition label
    unsigned long index;                // Last partition index
    fs_node_t *node;                    // Node

    partition_read_t read;              // Read method
    partition_write_t write;            // Write method
    void *d;                            // Driver-specific
} partition_t;


/**** FUNCTIONS ****/

/**
 * @brief Create a new partition on a drive
 * @param drive The drive to create the partition on
 * @param size Size of the partition
 */
partition_t *partition_create(struct drive *drive, size_t size);

/**
 * @brief Mount a partition
 * @param partition The partition to mount
 */
int partition_mount(partition_t *part);

#endif