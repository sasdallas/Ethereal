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
#include <kernel/drivers/storage/gpt.h>
#include <kernel/fs/devfs.h>
#include <kernel/mm/alloc.h>
#include <kernel/mm/vmm.h>
#include <kernel/debug.h>
#include <errno.h>
#include <string.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "STORAGE:DRIVE", __VA_ARGS__)

/* Stupid drive mappings */
static char* drive_mappings[] = {
    [DRIVE_TYPE_UNKNOWN]    = DRIVE_NAME_UNKNOWN,
    [DRIVE_TYPE_IDE_HD]     = DRIVE_NAME_IDE_HD,
    [DRIVE_TYPE_CDROM]      = DRIVE_NAME_CDROM,
    [DRIVE_TYPE_SATA]       = DRIVE_NAME_SATA,
    [DRIVE_TYPE_SCSI_CDROM] = DRIVE_NAME_SCSI_CDROM,
    [DRIVE_TYPE_NVME]       = DRIVE_NAME_NVME,
    [DRIVE_TYPE_FLOPPY]     = DRIVE_NAME_FLOPPY,
    [DRIVE_TYPE_MMC]        = DRIVE_NAME_MMC,
};

static int drive_ids[] = {
    [DRIVE_TYPE_UNKNOWN]    = 0,
    [DRIVE_TYPE_IDE_HD]     = 0,
    [DRIVE_TYPE_CDROM]      = 0,
    [DRIVE_TYPE_SATA]       = 0,
    [DRIVE_TYPE_SCSI_CDROM] = 0,
    [DRIVE_TYPE_NVME]       = 0,
    [DRIVE_TYPE_FLOPPY]     = 0,
    [DRIVE_TYPE_MMC]        = 0,
};

/* device filesystem ops */
static ssize_t drive_read(devfs_node_t *node, loff_t off, size_t size, char *buffer);
static ssize_t drive_write(devfs_node_t *node, loff_t off, size_t size, const char *buffer);
static int drive_ioctl(devfs_node_t *node, unsigned long request, void *argp);

static int drive_read_range(devfs_node_t *node, page_range_t *range);
static int drive_write_range(devfs_node_t *node, page_range_t *range);

static devfs_ops_t drive_ops = {
    .open = NULL,
    .close = NULL,
    .read = drive_read,
    .write = drive_write,
    .ioctl = drive_ioctl,
    .lseek = NULL,
    .poll = NULL,
    .poll_events = NULL,
    .mmap = NULL,
    .mmap_prepare = NULL,
    .munmap = NULL,
    .read_range = drive_read_range,
    .write_range = drive_write_range,
};

/**
 * @brief Drive ioctl
 */
static int drive_ioctl(devfs_node_t *node, unsigned long request, void *argp) {
    drive_t *d = node->priv;
    switch (request) {
        case IO_DRIVE_GET_INFO:
            drive_info_t *i = (drive_info_t*)argp;
            i->sector_size = d->sector_size;
            i->sectors = d->sectors;
            i->type = d->type;
            return 0;

        case IO_DRIVE_GET_MODEL:
            if (d->model) strncpy(argp, d->model, 256);
            return 0;

        case IO_DRIVE_GET_SERIAL:
            if (d->serial) strncpy(argp, d->serial, 256);
            return 0;

        case IO_DRIVE_GET_REV:
            if (d->revision) strncpy(argp, d->revision, 256);
            return 0;

        case IO_DRIVE_GET_VENDOR:
            if (d->vendor) strncpy(argp, d->vendor, 256);
            return 0;
        
        case IO_DRIVE_RESCAN_PART: {
            LOG(INFO, "Rescanning for partitions on drive.\n");

            // !!! Incredibly racey
            node_t *n = d->partitions->head;
            while (n) {
                node_t *nxt = n->next;
                partition_delete((partition_t*)n->value);
                n = nxt;
            }

            d->last_part_index = 0;

            drive_mount(d);
            return 0;
        }

        default:
            if (d->ops->ioctl) return d->ops->ioctl(d,request,argp);
            return -ENODEV;
    }
}

/**
 * @brief Generic drive read method
 * @param node The drive node to read
 * @param off The offset to read at
 * @param size The size to read
 * @param buffer The buffer to read into
 */
static ssize_t drive_read(devfs_node_t *node, loff_t off, size_t size, char *buffer) {
    if (!size) return 0;
    if (off > node->attr.size) return 0;
    if (off + size > (size_t)node->attr.size) size = node->attr.size - off;

    drive_t *d = (drive_t*)node->priv;

    // Calculate how many sectors it would span
    uint64_t lba_start = (uint64_t)off / d->sector_size;
    uint64_t sector_offset = (uint64_t)off % d->sector_size;
    uint64_t end = off + size;
    uint64_t end_sector = (end + d->sector_size - 1) / d->sector_size;
    uint64_t sector_count = end_sector - lba_start;

    // Trigger read call
    size_t bsz = sector_count * d->sector_size;
    uint8_t *temporary_buffer = vmm_map(NULL, bsz, VM_FLAG_ALLOC, MMU_FLAG_PRESENT|MMU_FLAG_WRITE);
    ssize_t r = d->ops->read_sectors(d, lba_start, sector_count, temporary_buffer);

    if (r != (ssize_t)sector_count) {
        vmm_unmap(temporary_buffer, bsz);
        return r;
    }

    // We now have a temporary buffer, copy at sector_offset to buffer
    memcpy(buffer, (temporary_buffer + sector_offset), size);
    vmm_unmap(temporary_buffer,bsz);

    return size;
}

/**
 * @brief Generic drive write method
 * @param node The drive node to write
 * @param off The offset to write
 * @param size The size to write
 * @param buffer The buffer to write
 */
static ssize_t drive_write(devfs_node_t *node, loff_t off, size_t size, const char *buffer) {
    if (!size) return 0;
    if (off > node->attr.size) return 0;
    if (off + size > (size_t)node->attr.size) size = node->attr.size - off;

    LOG(DEBUG, "drive_write off=%d size=%d\n", off, size);

    drive_t *d = (drive_t*)node->priv;

    // Calculate how many sectors it would span
    uint64_t lba_start = (uint64_t)off / d->sector_size;
    uint64_t sector_offset = (uint64_t)off % d->sector_size;
    uint64_t end = off + size;
    uint64_t end_sector = (end + d->sector_size - 1) / d->sector_size;
    uint64_t sector_count = end_sector - lba_start;

    // First, create a buffer to hold everything
    size_t bsz = sector_count * d->sector_size;
    uint8_t *write_buffer = vmm_map(NULL, bsz, VM_FLAG_ALLOC, MMU_FLAG_PRESENT|MMU_FLAG_WRITE);
    
    // Read the first sector if needed
    if (sector_offset) {
        ssize_t r = d->ops->read_sectors(d, lba_start, 1, write_buffer);
        if (r < 0) {
            vmm_unmap(write_buffer, bsz);
            return r;
        }

        // We also have to get the last sector
        r = d->ops->read_sectors(d, end_sector - 1, 1, write_buffer + ((sector_count-1) * d->sector_size));
        if (r < 0) {
            vmm_unmap(write_buffer, bsz);
            return r;
        }
    }

    // Copy into the write buffer
    memcpy(write_buffer + sector_offset, buffer, size);

    // Trigger write call
    ssize_t r = d->ops->write_sectors(d, lba_start, sector_count, write_buffer);

    if (r != (ssize_t)sector_count) {
        vmm_unmap(write_buffer, bsz);
        return r;
    }

    // We now have a temporary buffer, copy at sector_offset to buffer
    vmm_unmap(write_buffer, bsz);

    return size;
}

static int drive_read_range(devfs_node_t *node, page_range_t *range) {
    drive_t *d = (drive_t*)node->priv;

    ssize_t r = 0;
    if (d->ops->read_range) {
        r = d->ops->read_range(d, range);
    } else {
        assert(d->sector_size <= PAGE_SIZE && "unimpl");
        for (unsigned i = 0; i < range->npages; i++) {
            uintptr_t p = arch_mmu_remap_physical(range->pages[i], PAGE_SIZE, REMAP_TEMPORARY);
            ssize_t r = d->ops->read_sectors(d, range->offset + (i * PAGE_SIZE), PAGE_SIZE/d->sector_size, (uint8_t*)p);
            arch_mmu_unmap_physical(range->pages[i], PAGE_SIZE);
            
            if (r < 0) {
                return r;
            }
        }
    }

    return r;
}

static int drive_write_range(devfs_node_t *node, page_range_t *range) {
    drive_t *d = (drive_t*)node->priv;

    ssize_t r = 0;
    if (d->ops->write_range) {
        r = d->ops->write_range(d, range);
    } else {
        assert(d->sector_size <= PAGE_SIZE && "unimpl");
        for (unsigned i = 0; i < range->npages; i++) {
            uintptr_t p = arch_mmu_remap_physical(range->pages[i], PAGE_SIZE, REMAP_TEMPORARY);
            ssize_t r = d->ops->write_sectors(d, range->offset + (i * PAGE_SIZE), PAGE_SIZE/d->sector_size, (uint8_t*)p);
            arch_mmu_unmap_physical(range->pages[i], PAGE_SIZE);
            
            if (r < 0) {
                return r;
            }
        }
    }

    return r;
}


/**
 * @brief Create a new drive object
 * @param type The type of the drive being created
 * @param ops Drive operations
 * 
 * Please make sure you fill all fields possible out
 */
drive_t *drive_create(int type, drive_ops_t *ops) {
    if (type < 0 || type > DRIVE_TYPE_MMC) type = DRIVE_TYPE_UNKNOWN;
    drive_t *r = kzalloc(sizeof(drive_t));
    r->type = type;
    r->ops = ops;

    // Construct drive name
    char name_tmp[256];
    snprintf(name_tmp, 256, "%s%d", drive_mappings[type], drive_ids[type]++);
    r->node = devfs_register(devfs_root, name_tmp, VFS_BLOCKDEVICE, &drive_ops, DEVFS_MAJOR_IDE_HD, drive_ids[type], r); // TODO: DEVFS MAJOR SUPPORT 

    LOG(DEBUG, "Created new drive \"%s\"\n", name_tmp);
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
    drive->node->attr.size = drive->sectors * drive->sector_size;

    // Init MBR
    if (mbr_init(drive) == 0) {
        return 0;
    }

    // Init GPT
    if (gpt_init(drive) == 0) {
        return 0;
    }

    LOG(INFO, "No valid partitioning scheme detected on drive.\n");
    return 0;
}
