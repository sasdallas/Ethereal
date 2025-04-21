/**
 * @file hexahedron/include/kernel/drivers/clock.h
 * @brief Generic clock driver headers
 * 
 * @todo This structure kind of sucks.
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef DRIVERS_CLOCK_H
#define DRIVERS_CLOCK_H

/**** INCLUDES ****/
#include <stdint.h>
#include <time.h>

/**** TYPES ****/

// Functions for clock device
typedef uint64_t (*get_timer_t)(void);
typedef uint64_t (*get_timer_raw_t)(void);
typedef void (*get_tick_counts_t)(uint64_t, uint64_t*, uint64_t*); // This should take in current ticks, and output ticks and subticks via subdivision.
typedef void (*set_boottime_t)(uint64_t);
typedef void (*sleep_t)(uint64_t);

// Clock device vtable
typedef struct _clock_device_t {
    get_timer_t get_timer;              // Get timer function. This should be return a quotient equivalent to the architecture's clock speed.
    get_timer_raw_t get_timer_raw;      // Raw version. Shouldn't be used but is here.
    get_tick_counts_t get_tick_counts;  // Returns tick counts
                                        // TODO: Kill that with fire!
    
    set_boottime_t set_boottime;        // Sets the architecture boot-time.
                                        // TODO: Also kill this with fire!

    sleep_t sleep;                      // Sleep function (takes in ms)

    uint64_t boot_time; // Boot time of architecture
} clock_device_t;

/**
 * @brief Clock callback, called every update
 * @param ticks The current tick count
 */
typedef void (*clock_callback_t)(uint64_t ticks);

/**** DEFINITIONS ****/

#define MAX_CLOCK_CALLBACKS 128
#define SUBSECONDS_PER_SECOND 1000000

/**** FUNCTIONS ****/

/**
 * @brief Get the current time of day
 */
int clock_gettimeofday(struct timeval *t, void *z);

/**
 * @brief Set the current time of day
 */
int clock_settimeofday(struct timeval *t, void *z);

/**
 * @brief Relative time
 * @param seconds Seconds in the future
 * @param subseconds Subseconds in the future
 * @param out_seconds Absolute seconds in the future
 * @param out_subseconds Absolute subseconds in the future
 */
void clock_relative(unsigned long seconds, unsigned long subseconds, unsigned long *out_seconds, unsigned long *out_subseconds);

/**
 * @brief Update method. This should be called to by the architecture-based clock driver.
 * @param ticks Current amount of ticks returned by @c clock_getTickCount
 */
void clock_update(uint64_t ticks);

/**
 * @brief Get the current tick count
 * @returns The tick count
 */
uint64_t clock_getTickCount();

/**
 * @brief Register an update callback
 * @returns -EINVAL on too many full, else it returns the index.
 */
int clock_registerUpdateCallback(clock_callback_t callback);

/**
 * @brief Unregister a clock handler
 * @param index Index to unregister.
 */
void clock_unregisterUpdateCallback(int index);

/**
 * @brief Set the main clock device
 */
void clock_setDevice(clock_device_t device);

/**
 * @brief Get boot time
 */
uint64_t clock_getBoottime();

/**
 * @brief Get the current clock device
 */
clock_device_t clock_getDevice();

/**
 * @brief Returns whether the clock device is ready or not.
 * @todo This seems stupid. It is only used by debug system.
 */
int clock_isReady();

/**
 * @brief Sleep for a period of time
 * @param delay Delay to sleep in ms
 */
void clock_sleep(size_t delay);

/**
 * @brief Get the current seconds/subseconds according to the clock device
 * @param out_seconds Output variable for seconds
 * @param out_subseconds Output variable for subseconds
 */
void clock_getCurrentTime(unsigned long *out_seconds, unsigned long *out_subseconds);

#endif