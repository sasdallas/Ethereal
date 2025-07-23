/**
 * @file hexahedron/include/kernel/hal.h
 * @brief Generic HAL calls
 * 
 * Implements generic HAL calls, such as CPU stalling, clock ticks, etc.
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_HAL_H
#define KERNEL_HAL_H

/**** INCLUDES ****/
#include <stdint.h>

/**** TYPES ****/

/**
 * @brief Generic interrupt handler
 * @param context The context passed to @c hal_registerInterruptHandler
 */
typedef int (*hal_interrupt_handler_t)(void *context);

/**** FUNCTIONS ****/

/**
 * @brief Register an interrupt handler
 * @param int_number The interrupt number to register a handler for
 * @param handler The handler to register
 * @param context Optional context that gets passed to the handler
 * @returns 0 on success
 */
int hal_registerInterruptHandler(uintptr_t int_number, hal_interrupt_handler_t handler, void *context);

/**
 * @brief Unregisters an interrupt handler
 */
void hal_unregisterInterruptHandler(uintptr_t int_no);

#endif