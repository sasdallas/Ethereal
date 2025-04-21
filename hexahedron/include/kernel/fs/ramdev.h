/**
 * @file hexahedron/include/kernel/fs/ramdev.h
 * @brief RAM device header
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_FS_RAMDEV_H
#define KERNEL_FS_RAMDEV_H

/**** INCLUDES ****/

#include <stdint.h>
#include <kernel/fs/vfs.h>

/**** FUNCTIONS ****/

/**
 * @brief Mount RAM device
 * @param addr Virtual memory to use
 * @param size The size of the ramdev
 */
fs_node_t *ramdev_mount(uintptr_t addr, uintptr_t size);

#endif