/**
 * @file hexahedron/include/kernel/drivers/x86/pit.h
 * @brief PIT header
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef DRIVERS_X86_PIT_H
#define DRIVERS_X86_PIT_H

/**** INCLUDES ****/
#include <stdint.h>

/**** DEFINITIONS ****/

#define PIT_CHANNEL_A   0x40  // Main timer channel. Generates IRQ0
#define PIT_CHANNEL_B   0x41  // No longer usable
#define PIT_CHANNEL_C   0x42  // PC speaker
#define PIT_MODE        0x43  // Mode/command register

#define PIT_IRQ         0       // Channel A IRQ
#define PIT_SCALE       1193180 // 1.193180 MHz (round up of PIT 1.193182 MHz)

// Only bitmasks we need
#define PIT_RATE_GENERATOR  0x04 // 0 1 0 - Mode 2
#define PIT_LOBYTE_HIBYTE   0x30 // Access mode



/**** FUNCTIONS ****/

/**
 * @brief Change the PIT timer phase.
 * @warning Don't touch unless you know what you're doing.
 */
void pit_setTimerPhase(long hz);

/**
 * @brief Initialize PIT
 */
void pit_initialize();

/**
 * @brief Sleep function
 */
void pit_sleep(uint64_t ms);

/**
 * @brief Change the PIT state. 
 * 
 * This is used when the LAPIC timer is initialized, to allow the PIT to still operate as a timer without scheduling
 */
void pit_setState(int enabled);


#endif