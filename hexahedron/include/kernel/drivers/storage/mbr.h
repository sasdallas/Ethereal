/**
 * @file hexahedron/include/kernel/drivers/storage/mbr.h
 * @brief MBR
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef DRIVERS_STORAGE_MBR_H
#define DRIVERS_STORAGE_MBR_H

/**** INCLUDES ****/
#include <stdint.h>
#include <kernel/misc/util.h>

/**** TYPES ****/

struct drive;

typedef struct mbr_part_entry {
    uint8_t attrib;                     // Attributes
    uint8_t chs[3];                     // CHS
    uint8_t type;                       // Partition type
    uint8_t chs_end[3];                 // Ending CHS
    uint32_t lba;                       // LBA start
    uint32_t sector_count;              // Sector count
} __attribute__((packed)) mbr_part_entry_t;

STATIC_ASSERT(sizeof(mbr_part_entry_t) == 16);

typedef struct mbr_header {
    char bootstrap[440];                // MBR code
    uint32_t disk_id;                   // Disk ID
    uint16_t reserved;                  // Reserved
    mbr_part_entry_t entries[4];        // Entries
    uint16_t signature;                 // 0xAA55
} mbr_header_t;

STATIC_ASSERT(sizeof(mbr_header_t) == 512);

typedef struct mbr_partition {
    uint8_t type;                       // Type of partition
    uint32_t offset;                    // The offset to send to drive reading
    size_t size;                        // Size in bytes of the partition 
} mbr_partition_t;

/**** FUNCTIONS ****/

/**
 * @brief Try to initialize MBR on a drive
 * @param drive The drive to initialize MBR on
 * @returns 1 if MBR could be initialized
 */
int mbr_init(struct drive *drive);

#endif