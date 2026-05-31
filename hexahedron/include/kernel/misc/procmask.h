/**
 * @file hexahedron/include/kernel/misc/procmask.h
 * @brief Processor mask (procmask_t)
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef KERNEL_MISC_PROCMASK_H
#define KERNEL_MISC_PROCMASK_H

/**** INCLUDES ****/
#include <kernel/config.h>
#include <structs/bitmap.h>

/**** TYPES ****/

typedef struct __procmask {
    BITMAP_DEFINE(bm, MAX_CPUS);
} __procmask_t;

// TODO: add the ability to offload this to slab
typedef __procmask_t procmask_t;

/**** MACROS ****/

#define procmask_setall(mask) bitmap_fill((mask)->bm, 0xFF, MAX_CPUS)
#define procmask_clear(mask) bitmap_fill((mask)->bm, 0, MAX_CPUS)
#define procmask_set(mask, cpu) bitmap_set((mask)->bm, cpu)
#define procmask_unset(mask, cpu) bitmap_clear((mask)->bm, cpu)
#define procmask_test(mask, cpu) bitmap_test((mask)->bm, cpu)

#endif
