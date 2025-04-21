/**
 * @file hexahedron/include/kernel/fs/null.h
 * @brief Null/zero device (/device/null and /device/zero)
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_FS_NULL_H
#define KERNEL_FS_NULL_H


/**** INCLUDES ****/
#include <stdint.h>
#include <kernel/fs/vfs.h>

/**** DEFINITIONS ****/

#define NULLDEV_MOUNT_PATH      "/device/null"
#define ZERODEV_MOUNT_PATH      "/device/zero"

/**** FUNCTIONS ****/

/**
 * @brief Initialize the null device and mounts it
 */
void nulldev_init();

/**
 * @brief Initialize the zero device and mounts it
 */
void zerodev_init();

#endif