/**
 * @file hexahedron/include/kernel/subsystems/clock.h
 * @brief Clock/timekeeper subsystem
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef KERNEL_SUBSYSTEMS_CLOCK_H
#define KERNEL_SUBSYSTEMS_CLOCK_H

/**** INCLUDES ****/
#include <stddef.h>
#include <stdint.h>

/**** DEFINITIONS ****/

// replacement for struct timestamp
typedef struct {
    long s;
    long ns;
} timestamp_t;

/**** TYPES ****/


typedef struct clock_device {
    struct clock_device *next;
    char *name;
    unsigned char priority;
    uint64_t freq_hz;

    struct {
        int (*init)(struct clock_device *);
        int (*deinit)(struct clock_device *);
        uint64_t (*ticks)(struct clock_device *);
    } ops;

    void *private;
} clock2_device_t;

/* per-CPU timekeeper */
typedef struct timekeeper {
    clock2_device_t *clock;

    // tick_skew is unused for now but normally it would be used to provide a time since boot calculation.
    // the calculation is performed as (tk->clock->ops->ticks() / tk->clock->freq_hz + tk->tick_skew) - tk->initial for time since boot
    int64_t tick_skew;
    uint64_t initial;
} timekeeper_t;

/**** MACROS ****/

#define TIMESTAMP_ADD(a,b,dst) do { \
        (dst)->s = (a)->s + (b)->s; \
        (dst)->ns = (a)->ns + (b)->ns; \
        if ((dst)->ns >= 1000000000L) { \
            (dst)->s++; \
            (dst)->ns -= 1000000000L; \
        } \
    } while (0)

/**** FUNCTIONS ****/

/**
 * @brief Initialize clock for this CPU
 */
int clock2_init();

/**
 * @brief Register a clock device
 * @param device The device to register
 */
void clock_registerDevice(clock2_device_t *dev);

/**
 * @brief Get the time since boot
 */
timestamp_t clock_boottime();

/**
 * @brief Get the current time of day
 */
timestamp_t clock_time();

/**
 * @brief Get the raw UNIX timestamp for system boot
 */
uint64_t clock_getBootTime();

#endif
