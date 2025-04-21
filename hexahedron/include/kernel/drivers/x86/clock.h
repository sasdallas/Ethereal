/**
 * @file hexahedron/include/kernel/arch/i386/clock.h
 * @brief I386 clock header
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef DRIVERS_X86_CLOCK_H
#define DRIVERS_X86_CLOCK_H

/**** INCLUDES ****/
#include <stdint.h>
#include <stddef.h>

/**** DEFINITIONS ****/

#define CMOS_ADDRESS    0x70    // CMOS I/O address
#define CMOS_DATA       0x71    // CMOS data address

/**** TYPES ****/
enum {
    CMOS_SECOND = 0,
    CMOS_MINUTE = 2,
    CMOS_HOUR = 4,
    CMOS_DAY = 7,
    CMOS_MONTH = 8,
    CMOS_YEAR = 9
};


/**** MACROS ****/
#define from_bcd(n) ((n / 16) * 10 + (n & 0xF)) // who the hell thought it would be a good idea to make CMOS use BCD
                                                // BCD is terrible

/**** FUNCTIONS ****/

/**
 * @brief Initialize the CMOS-based clock driver.
 */
void clock_initialize();

/**
 * @brief Read the current CPU timestamp counter.
 */
uint64_t clock_readTSC();

/**
 * @brief Get the TSC speed
 */
size_t clock_getTSCSpeed();

/**
 * @brief Gets the tick count (CPU timestamp counter / TSC speed)
 * 
 * You should probably use this in timers with clock_update(clock_readTicks())
 */
uint64_t clock_readTicks();

#endif