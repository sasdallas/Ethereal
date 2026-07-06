/**
 * @file hexahedron/drivers/storage/gpt.c
 * @brief GPT partitioning driver
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#include <kernel/drivers/storage/drive.h>
#include <kernel/drivers/storage/mbr.h>
#include <kernel/drivers/storage/gpt.h>
#include <kernel/mm/alloc.h>
#include <kernel/debug.h>
#include <string.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "STORAGE:GPT", __VA_ARGS__)

#define FIX_GUID(guid) ({ char sv = guid[9]; guid[9] = guid[10]; guid[10] = sv; })
char zeroguid[16];

/**
 * @brief Try to initialize GPT on a drive
 * @returns 0 on success
 */
int gpt_init(drive_t *drive) {
    // Read the GPT partition table header
    // !!! Uses hardcoded logical size 512
    gpt_header_t hdr;
    ssize_t r = drive->node->ops->read(drive->node, 512, sizeof(gpt_header_t), (char*)&hdr); // I ain't doing that VFS open shit
    if (r != sizeof(gpt_header_t)) {
        LOG(WARN, "Failed to read GPT header (%d)\n", r);
        return r;
    }

    if (memcmp(hdr.signature, "EFI PART", 8)) {
        LOG(INFO, "No GPT partition detected.\n");
        return -EINVAL;
    }

    LOG(DEBUG, "GPT: LBA start is %d, %d partitions each %d bytes in length\n", hdr.lba_start, hdr.npart, hdr.partsize);

    // Confirmed that this is a GPT partition
    size_t sz = hdr.npart * hdr.partsize;
    char *tbl = kzalloc(sz);
    r = drive->node->ops->read(drive->node, 512*hdr.lba_start, sz, (char*)tbl);
    if (r < 0) {
        LOG(ERR, "Failed to read GPT partition information (%d)\n", r);
        kfree(tbl);
        return r;
    }

    for (unsigned i = 0; i < hdr.npart; i++) {
        gpt_part_entry_t *ent = (gpt_part_entry_t*)((uintptr_t)tbl + (i * hdr.partsize));
        if (!memcmp(ent->guid, zeroguid, 16)) continue;

        FIX_GUID(ent->guid);
        FIX_GUID(ent->typeguid);

        // TODO: store typeguid for automount
        size_t partsize = (ent->lba_end-ent->lba_start);
        size_t partoff = ent->lba_start;
        LOG(DEBUG, "GPT partition partsize=%d partoff=%d\n", partsize, partoff);
        partition_t *p = partition_create(drive, partoff, partsize, NULL);
        memcpy(p->guid, ent->guid, 16);
    }

    kfree(tbl);

    return 0;
}
