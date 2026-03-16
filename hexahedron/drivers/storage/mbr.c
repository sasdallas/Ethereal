/**
 * @file hexahedron/drivers/storage/mbr.c
 * @brief MBR (master boot record) partition driver
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


/* Log method */
#define LOG(status, ...) dprintf_module(status, "STORAGE:MBR", __VA_ARGS__)

static ssize_t mbr_write(partition_t *part, loff_t offset, size_t size, const char *buffer);
static ssize_t mbr_read(partition_t *part, loff_t offset, size_t size, char *buffer);

static partition_ops_t mbr_ops = {
    .read = mbr_read,
    .write = mbr_write,
};

/**
 * @brief MBR read method
 */
static ssize_t mbr_read(partition_t *part, loff_t offset, size_t size, char *buffer) {
    mbr_partition_t *p2 = (mbr_partition_t*)part->d;
    if (offset > (off_t)p2->size) return 0;
    if (size + offset > p2->size) size = p2->size - offset;

    return part->parent->node->ops->read(part->parent->node, p2->offset + offset, size, buffer); // !!!
}

/**
 * @brief MBR write method
 */
static ssize_t mbr_write(partition_t *part, loff_t offset, size_t size, const char *buffer) {
    mbr_partition_t *p2 = (mbr_partition_t*)part->d;
    if (offset > (off_t)p2->size) return 0;
    if (size + offset > p2->size) size = p2->size - offset;

    return part->parent->node->ops->write(part->parent->node, p2->offset + offset, size, buffer); // !!!
}

/**
 * @brief Try to initialize MBR on a drive
 * @param drive The drive to initialize MBR on
 * @returns 1 if MBR could be initialized
 */
int mbr_init(struct drive *drive) {
    // First, read the MBR header in
    mbr_header_t mbr_header;
    ssize_t r = drive->node->ops->read(drive->node, 0, sizeof(mbr_header_t), (char*)&mbr_header); // I ain't doing that VFS open shit
    if (r != sizeof(mbr_header_t)) {
        return 0;
    }

    // Check signature
    if (mbr_header.signature != 0xAA55) {
        return 0;
    }

    // MBR partition detected
    for (int i = 0; i < 4; i++) {
        LOG(DEBUG, "MBR partition attr %x type %x LBA %d - %d\n", mbr_header.entries[i].attrib, mbr_header.entries[i].type, mbr_header.entries[i].lba, mbr_header.entries[i].lba + mbr_header.entries[i].sector_count);
    
        // Now, bootable partitions have the active bit set, but normal partitions also just have a nonzero type,.
        if (mbr_header.entries[i].type) {
            if (mbr_header.entries[i].type == 0xEE) {
                // TODO: Not sure if MBR partitions can still exist?
                LOG(WARN, "GPT partition detected\n");
                return 0;
            }

            // Allocate new partition
            mbr_partition_t *part = kmalloc(sizeof(mbr_partition_t));
            part->type = mbr_header.entries[i].type;
            part->size = mbr_header.entries[i].sector_count * drive->sector_size;
            part->offset = mbr_header.entries[i].lba * drive->sector_size;
            partition_create(drive, part->size, &mbr_ops, part);
        }
    }

    return 1;
}