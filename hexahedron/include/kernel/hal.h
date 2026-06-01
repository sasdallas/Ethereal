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

/**** DEFINITIONS ****/

#define HAL_INTERRUPTS_DISABLED         0
#define HAL_INTERRUPTS_ENABLED          1

#define HAL_POWER_SHUTDOWN              1
#define HAL_POWER_REBOOT                2
#define HAL_POWER_HIBERNATE             3

/**** FUNCTIONS ****/

/**
 * @brief Set interrupt state on the current CPU
 * @param state The interrupt state to set
 */
void hal_setInterruptState(int state);

/**
 * @brief Get the interrupt state on the current CPU
 */
int hal_getInterruptState();

/**
 * @brief Set power state
 * @param state The power state to set
 * @returns Error code
 */
int hal_setPowerState(int state);

/**
 * @brief Prepare for power state change
 * @param state The state
 */
void hal_prepareForPowerState(int state);

#endif