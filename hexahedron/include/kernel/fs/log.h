/**
 * @file hexahedron/include/kernel/fs/log.h
 * @brief Log device
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef KERNEL_FS_LOG_H
#define KERNEL_FS_LOG_H

/**** INCLUDES ****/
#include <stdint.h>

/**** FUNCTIONS ****/

/**
 * @brief Mount the log device
 */
void log_mount();

#endif