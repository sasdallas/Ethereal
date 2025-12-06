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

/**
 * @brief MBR read method
 */
ssize_t mbr_read(partition_t *part, off_t offset, size_t size, uint8_t *buffer) {
    mbr_partition_t *p2 = (mbr_partition_t*)part->d;
    if (offset > (off_t)p2->size) return 0;
    if (size + offset > p2->size) size = p2->size - offset;

    return fs_read(part->parent->node, p2->offset + offset, size, buffer);
}

/**
 * @brief MBR write method
 */
ssize_t mbr_write(partition_t *part, off_t offset, size_t size, uint8_t *buffer) {
    mbr_partition_t *p2 = (mbr_partition_t*)part->d;
    if (offset > (off_t)p2->size) return 0;
    if (size + offset > p2->size) size = p2->size - offset;

    return fs_write(part->parent->node, p2->offset + offset, size, buffer);
}

/**
 * @brief Try to initialize MBR on a drive
 * @param drive The drive to initialize MBR on
 * @returns 1 if MBR could be initialized
 */
int mbr_init(struct drive *drive) {
    // First, read the MBR header in
    mbr_header_t *mbr_header = kzalloc(sizeof(mbr_header_t));
    ssize_t r = fs_read(drive->node, 0, sizeof(mbr_header_t), (uint8_t*)mbr_header);
    if (r != sizeof(mbr_header_t)) {
        return 0; // No space for header/I/O error
    }

    // Check signature
    if (mbr_header->signature != 0xAA55) {
        // LOG(ERR, "Signature failure: 0x%x\n", mbr_header->signature);
        return 0;
    }

    // MBR partition detected
    for (int i = 0; i < 4; i++) {
        LOG(DEBUG, "MBR partition attr %x type %x LBA %d - %d\n", mbr_header->entries[i].attrib, mbr_header->entries[i].type, mbr_header->entries[i].lba, mbr_header->entries[i].lba + mbr_header->entries[i].sector_count);
    
        // Now, bootable partitions have the active bit set, but normal partitions also just have a nonzero type,.
        if (mbr_header->entries[i].type) {
            if (mbr_header->entries[i].type == 0xEE) {
                // TODO: Not sure if MBR partitions can still exist?
                LOG(WARN, "GPT partition detected\n");
                return 0;
            }

            // Allocate new partition
            mbr_partition_t *part = kmalloc(sizeof(mbr_partition_t));
            part->type = mbr_header->entries[i].type;
            part->size = mbr_header->entries[i].sector_count * drive->sector_size;
            part->offset = mbr_header->entries[i].lba * drive->sector_size;

            partition_t *partition = partition_create(drive, part->size);
            partition->d = part;
            partition->label = NULL;
            partition->read = mbr_read;
            partition->write = mbr_write;

            partition_mount(partition);
        }
    }

    return 1;
}