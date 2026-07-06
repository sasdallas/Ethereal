/**
 * @file hexahedron/include/kernel/drivers/storage/gpt.h
 * @brief GPT partition support
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#ifndef DRIVERS_STORAGE_GPT_H
#define DRIVERS_STORAGE_GPT_H

/**** INCLUDES ****/
#include <stdint.h>

/**** TYPES ****/

typedef struct gpt_header {
    char signature[8];
    uint32_t revision;
    uint32_t size;
    uint32_t crc;
    uint32_t rsvd;
    uint64_t lba;
    uint64_t altlba;
    uint64_t firstblk;
    uint64_t lastblk;
    char guid[16];
    uint64_t lba_start;
    uint32_t npart;
    uint32_t partsize;
    uint32_t partcrc;
} gpt_header_t;

typedef struct gpt_part_entry {
    char typeguid[16];
    char guid[16];
    uint64_t lba_start;
    uint64_t lba_end;
    uint64_t attr;
    char name[72];
} gpt_part_entry_t;


/**** FUNCTIONS ****/

/**
 * @brief Try to initialize GPT on a drive
 * @returns 0 on success
 */
int gpt_init(struct drive *drive);

#endif
