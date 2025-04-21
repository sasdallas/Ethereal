/**
 * @file hexahedron/drivers/clock.c
 * @brief Generic clock driver
 * 
 * @todo Device checks! We can easily crash here if not careful!!!
 * @todo Maybe make clock_device a pointer. Memory is key though.
 * @warning This driver sucks.
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <kernel/drivers/clock.h>
#include <kernel/debug.h>

#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>

clock_device_t clock_device;

/* Tick count */
uint64_t tick_count = 0;

/* Whether a clock interface has been set */
static int is_ready = 0;

/* Callback table */
static clock_callback_t clock_callback_table[MAX_CLOCK_CALLBACKS] = { 0 };

/* Log method */
#define LOG(status, ...) dprintf_module(status, "CLOCK", __VA_ARGS__)

/**
 * @brief Update method. This should be called to by the architecture-based clock driver.
 */
void clock_update(uint64_t ticks) {
    // Store ticks for reference
    tick_count = ticks;

    // Handle all of the callbacks
    for (int i = 0; i < MAX_CLOCK_CALLBACKS; i++) {
        if (clock_callback_table[i]) {
            clock_callback_t callback = clock_callback_table[i];
            callback(ticks);
        }
    }
}

/**
 * @brief Get the current tick count
 * @returns The tick count
 */
uint64_t clock_getTickCount() {
    return tick_count;
}

/**
 * @brief Sleep for a period of time
 * @param delay Delay to sleep in ms
 */
void clock_sleep(size_t delay) {
    // !!!: hacked in
    if (!clock_device.sleep) {
        LOG(ERR, "clock_sleep called before clock initialized\n");
        return;
    }

    clock_device.sleep(delay);
}

/**
 * @brief Get the current time of day
 */
int clock_gettimeofday(struct timeval *t, void *z) {
    if (!t) return -EINVAL;
    
    uint64_t ticks = clock_device.get_timer();
    uint64_t timer_ticks, timer_subticks;
    if (clock_device.get_tick_counts) {
        clock_device.get_tick_counts(ticks, &timer_ticks, &timer_subticks);
    } else {
        LOG(ERR, "clock_gettimeofday called before clock initialized\n");
        return -EINVAL;
    }

    t->tv_sec = clock_device.boot_time + timer_ticks;
    t->tv_usec = timer_subticks;

    return 0;
}


/**
 * @brief Set the current time
 */
int clock_settimeofday(struct timeval *t, void *z) {
    if (!t) return -EINVAL;
    if (t->tv_usec > 1000000) return -EINVAL;

    uint64_t clock_time = now();
    clock_device.boot_time = clock_device.boot_time + (t->tv_sec - clock_time);
    clock_device.set_boottime(clock_device.boot_time);

    return 0;
}

/**
 * @brief Gets relative timing
 */
void clock_relative(unsigned long seconds, unsigned long subseconds, unsigned long *out_seconds, unsigned long *out_subseconds) {
    uint64_t ticks = clock_device.get_timer();
    uint64_t timer_ticks, timer_subticks;
    if (clock_device.get_tick_counts) {
        clock_device.get_tick_counts(ticks, &timer_ticks, &timer_subticks);
    } else {
        LOG(ERR, "clock_relative called before clock initialized\n");
        return;
    }

    // Now calculate
    if (subseconds + timer_ticks >= SUBSECONDS_PER_SECOND) {
        *out_seconds = timer_ticks + seconds + (subseconds + timer_subticks) / SUBSECONDS_PER_SECOND;
        *out_subseconds = (subseconds + timer_subticks) % SUBSECONDS_PER_SECOND;
    } else {
        *out_seconds = timer_ticks + seconds;
        *out_subseconds = timer_subticks + subseconds;
    }
}

/**
 * @brief Get the current seconds/subseconds according to the clock device
 * @param out_seconds Output variable for seconds
 * @param out_subseconds Output variable for subseconds
 */
void clock_getCurrentTime(unsigned long *out_seconds, unsigned long *out_subseconds) {
    uint64_t ticks = clock_device.get_timer();
    uint64_t timer_ticks, timer_subticks;
    if (clock_device.get_tick_counts) {
        clock_device.get_tick_counts(ticks, &timer_ticks, &timer_subticks);
    } else {
        LOG(ERR, "clock_getCurrentTime called before clock initialized");
        return;
    }

    *out_seconds = timer_ticks;
    *out_subseconds = timer_subticks;
}

/**
 * @brief Register an update callback
 * @returns -EINVAL on too many full, else it returns the index.
 */
int clock_registerUpdateCallback(clock_callback_t callback) {
    if (callback == NULL) return -EINVAL;
    
    // Find the first free one
    int first_free = -1; 
    for (int i = 0; i < MAX_CLOCK_CALLBACKS; i++) {
        if (!(clock_callback_table[i])) {
            first_free = i;
            break;
        }
    }  

    // Did we find it?
    if (first_free == -1) {
        return -EINVAL;
    }

    clock_callback_table[first_free] = callback;
    return first_free;
}

/**
 * @brief Unregister a clock handler
 * @param index Index to unregister.
 */
void clock_unregisterUpdateCallback(int index) {
    if (index < 0) return;
    clock_callback_table[index] = 0;
}

/**
 * @brief Get boot time
 */
uint64_t clock_getBoottime() {
    return clock_device.boot_time;
}

/**
 * @brief Get the current clock device
 */
clock_device_t clock_getDevice() {
    return clock_device;
}

/**
 * @brief Set the main clock device
 */
void clock_setDevice(clock_device_t device) {
    clock_device = device;
    is_ready = 1;
}


/**
 * @brief Returns whether the clock device is ready or not.
 * @todo This seems stupid. It is only used by debug system.
 */
int clock_isReady() {
    return is_ready;
}
