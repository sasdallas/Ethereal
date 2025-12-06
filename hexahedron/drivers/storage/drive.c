/**
 * @file hexahedron/drivers/storage/drive.c
 * @brief Drive API for Hexahedron
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
#include <kernel/drivers/storage/mbr.h>
#include <kernel/fs/drivefs.h>
#include <kernel/mm/alloc.h>
#include <kernel/debug.h>
#include <errno.h>
#include <string.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "STORAGE:DRIVE", __VA_ARGS__)

/**
 * @brief Generic drive read method
 * @param node The drive node to read
 * @param off The offset to read at
 * @param size The size to read
 * @param buffer The buffer to read into
 */
ssize_t drive_read(fs_node_t *node, off_t off, size_t size, uint8_t *buffer) {
    if (!size) return 0;
    if (off > (off_t)node->length) return 0;
    if (off + size > node->length) size = node->length - off;

    drive_t *d = (drive_t*)node->dev;

    // Calculate how many sectors it would span
    uint64_t lba_start = (uint64_t)off / d->sector_size;
    uint64_t sector_offset = (uint64_t)off % d->sector_size;
    uint64_t end = off + size;
    uint64_t end_sector = (end + d->sector_size - 1) / d->sector_size;
    uint64_t sector_count = end_sector - lba_start;

    // Trigger read call
    uint8_t *temporary_buffer = kmalloc(sector_count * d->sector_size);
    ssize_t r = d->read_sectors(d, lba_start, sector_count, temporary_buffer);

    if (r != (ssize_t)sector_count) {
        kfree(temporary_buffer);
        return r;
    }

    // We now have a temporary buffer, copy at sector_offset to buffer
    memcpy(buffer, (temporary_buffer + sector_offset), size);
    kfree(temporary_buffer);

    return size;
}

/**
 * @brief Generic drive write method
 * @param node The drive node to write
 * @param off The offset to write
 * @param size The size to write
 * @param buffer The buffer to write
 */
ssize_t drive_write(fs_node_t *node, off_t off, size_t size, uint8_t *buffer) {
    if (!size) return 0;
    if (off > (off_t)node->length) return 0;
    if (off + size > node->length) size = node->length - off;

    drive_t *d = (drive_t*)node->dev;

    // Calculate how many sectors it would span
    uint64_t lba_start = (uint64_t)off / d->sector_size;
    uint64_t sector_offset = (uint64_t)off % d->sector_size;
    uint64_t end = off + size;
    uint64_t end_sector = (end + d->sector_size - 1) / d->sector_size;
    uint64_t sector_count = end_sector - lba_start;

    // First, create a buffer to hold everything
    uint8_t *write_buffer = kmalloc(sector_count * d->sector_size);
    
    // Read the first sector if needed
    if (sector_offset) {
        ssize_t r = d->read_sectors(d, lba_start, 1, write_buffer);
        if (r < 0) {
            kfree(write_buffer);
            return r;
        }

        // We also have to get the last sector
        r = d->read_sectors(d, end_sector - 1, 1, write_buffer + ((sector_count-1) * d->sector_size));
        if (r < 0) {
            kfree(write_buffer);
            return r;
        }
    }

    // Copy into the write buffer
    memcpy(write_buffer + sector_offset, buffer, size);

    // Trigger write call
    ssize_t r = d->write_sectors(d, lba_start, sector_count, write_buffer);

    if (r != (ssize_t)sector_count) {
        kfree(write_buffer);
        return r;
    }

    // We now have a temporary buffer, copy at sector_offset to buffer
    kfree(write_buffer);

    return size;
}

/**
 * @brief Create a new drive object
 * @param type The type of the drive being created
 * 
 * Please make sure you fill all fields possible out
 */
drive_t *drive_create(int type) {
    drive_t *r = kzalloc(sizeof(drive_t));
    r->type = type;
    r->node = fs_node();

    strcpy(r->node->name, "drive");
    r->node->mask = 0600;
    r->node->flags = VFS_BLOCKDEVICE;
    r->node->atime = r->node->ctime = r->node->mtime = now();
    r->node->read = drive_read;
    r->node->write = drive_write;
    r->node->dev = (void*)r;

    r->partitions = list_create("part list");
    r->last_part_index = 0;

    return r;
}

/**
 * @brief Mount the drive object
 * @param drive The drive object to mount
 * @returns 0 on success
 */
int drive_mount(drive_t *drive) {
    // Update length
    drive->node->length = drive->sectors * drive->sector_size;

    // Mount node using DriveFS
    // TODO: Kill DriveFS
    drive->drivefs = drive_mountNode(drive->node, drive->type);
    
    // Init MBR
    if (mbr_init(drive) != 1) {
        LOG(DEBUG, "MBR initialization failed. GPT support is TODO\n");
    }

    return 0;
}