/**
 * @file hexahedron/include/kernel/fs/random.h
 * @brief /device/random
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef KERNEL_FS_RANDOM_H
#define KERNEL_FS_RANDOM_H

/**** INCLUDES ****/
#include <kernel/fs/vfs.h>

/**** FUNCTIONS ****/

/**
 * @brief Mount random device
 */
void random_mount();

#endif 