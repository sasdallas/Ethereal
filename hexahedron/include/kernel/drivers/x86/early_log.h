/**
 * @file hexahedron/include/kernel/drivers/x86/early_log.h
 * @brief Early log device
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef DRIVERS_X86_EARLY_LOG_H
#define DRIVERS_X86_EARLY_LOG_H

/**** INCLUDES ****/
#include <kernel/debug.h>
#include <kernel/arch/arch.h>

/**** FUNCTIONS ****/

/**
 * @brief Initialize the early log
 */
void earlylog_init();

#endif