/**
 * @file hexahedron/include/kernel/hal.h
 * @brief Generic HAL calls
 * 
 * Implements generic HAL calls, such as CPU stalling, clock ticks, etc.
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_HAL_H
#define KERNEL_HAL_H

/**** TYPES ****/

struct _registers;
struct _extended_registers;

/**
 * @brief Get registers from architecture
 * @returns Registers structure
 */
extern struct _registers *hal_getRegisters();



#endif