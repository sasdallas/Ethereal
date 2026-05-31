/**
 * @file hexahedron/subsystems/clock.c
 * @brief Kernel clock subsystem
 * 
 * The clock/timekeeper subsystem represents a method for Hexahedron to track time
 * Devices are registered, then @c clock_init is called which performs:
 * - Boot time calibration
 * - Clock source selection
 * 
 * @todo    Skew calculation, on platforms like x86_64 where the TSC is not synchronized between CPUs they need to be calculated.
 *          This involves a synchronized process
 * 
 * @warning Nothing in this will ever be 100% accurate.
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/subsystems/clock.h>
#include <kernel/arch/arch.h>
#include <kernel/misc/spinlock.h>
#include <kernel/processor_data.h>
#include <kernel/debug.h>
#include <kernel/panic.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "CLOCK", __VA_ARGS__)

/* Clock device list */
static clock2_device_t *clock_devices = NULL;
spinlock_t clock_device_lock = SPINLOCK_INITIALIZER;

/* Global UNIX boot time */
static uint64_t unix_boot_time = 0;

/* Has BSP done initialization */
static bool has_bsp_initted = false;

#define this_timekeeper &((processor_t*)&(processor_data[arch_current_cpu()]))->timekeeper


/**
 * @brief Get the time since boottime
 */
timestamp_t clock_boottime() {
    timekeeper_t *tk = this_timekeeper;

    uint64_t ticks = tk->clock->ops.ticks(tk->clock);
    uint64_t nanoseconds = ((ticks - tk->initial) * 1000000000UL) / tk->clock->freq_hz;
    return (timestamp_t){
        .s = nanoseconds / 1000000000UL,
        .ns = nanoseconds % 1000000000UL
    };
}

/**
 * @brief Get the raw UNIX timestamp for system boot
 */
uint64_t clock_getBootTime() {
    return unix_boot_time;
}


/**
 * @brief Get the current time of day
 */
timestamp_t clock_time() {
    // Get the time since initial and add unix_boot_time
    timekeeper_t *tk = this_timekeeper;

    uint64_t ticks = tk->clock->ops.ticks(tk->clock);
    uint64_t nanoseconds = ((ticks - tk->initial) * 1000000000UL) / tk->clock->freq_hz;
    return (timestamp_t){
        .s = (nanoseconds / 1000000000UL) + unix_boot_time,
        .ns = nanoseconds % 1000000000UL
    };
}

/**
 * @brief Register a clock device
 * @param device The device to register
 */
void clock_registerDevice(clock2_device_t *dev) {
    assert(!has_bsp_initted && "Clock source switching is not implemented");
    spinlock_acquire(&clock_device_lock);
    dev->next = clock_devices;
    clock_devices = dev;
    spinlock_release(&clock_device_lock);
}

/**
 * @brief Initialize clock for this CPU
 */
int clock2_init() {
    // Find the early init source
    timekeeper_t *tk = this_timekeeper;

    // Check if we are the BSP
    if (!has_bsp_initted) {
        // unix_boot_time = arch_get_boot_time();
        has_bsp_initted = true;
    }
    
    // Find the most optimal clock device
    spinlock_acquire(&clock_device_lock);

    clock2_device_t *best = NULL;
    clock2_device_t *i = clock_devices;
    while (i) {
        if ((!best || (best->priority < i->priority)) && (i->priority != 0)) {
            best = i;
        }

        i = i->next;
    }

    spinlock_release(&clock_device_lock);

    if (!i) {
        kernel_panic_extended(INSUFFICIENT_HARDWARE_ERROR, "clock", "*** No valid clock device was detected.\n");
    }

    // Try to initialize
    int status = best->ops.init(best);
    if (status != 0) {
        LOG(WARN, "Failed to initialize clock device \"%s\" (status code %d)\n", best->name, status);
        best->priority = 0; // setting the priority to 0 means that clock_init won't use this source
        return clock2_init();
    }

    tk->initial = best->ops.ticks(best);
    tk->clock = best;
    tk->tick_skew = 0; // TODO
 
    LOG(INFO, "Clock device \"%s\" selected as primary timekeeper (%lu Hz, %lu base ticks)\n", best->name, best->freq_hz, tk->initial);

    return 0;
}
