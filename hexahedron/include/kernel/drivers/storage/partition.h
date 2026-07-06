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

/**** TYPES ****/

struct drive;
struct partition;

typedef struct partition_ops {
    ssize_t (*read)(struct partition *part, loff_t off, size_t size, char *buffer);
    ssize_t (*write)(struct partition *part, loff_t off, size_t size, const char *buffer);
    void (*free)(struct partition *part);
} partition_ops_t;

typedef struct partition {
    node_t lnode;                       // List node
    struct drive *parent;               // Parent drive
    size_t size;                        // Size of partition
    loff_t off;                         // Partition offset
    char *label;                        // Optional partition label
    unsigned long index;                // Last partition index
    devfs_node_t *node;                 // Device filesystem node
    char guid[16];                      // Partition GUID

    
    void *d;                            // Driver-specific
} partition_t;


/**** FUNCTIONS ****/


/**
 * @brief Create a new partition on a drive
 * @param drive The drive to create the partition on
 * @param start_lba Start LBA of the partition
 * @param size Size of the partition
 * @param d Private data variable
 */
partition_t *partition_create(struct drive *drive, loff_t start_lba, size_t size_lba, void *d);

/**
 * @brief Unmount and delete a partition
 * @param part The partition to delete
 */
void partition_delete(partition_t *part);


#endif