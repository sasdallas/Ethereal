/**
 * @file hexahedron/include/kernel/subsystems/timer.h
 * @brief Timer subsystem
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef KERNEL_SUBSYSTEMS_TIMER_H
#define KERNEL_SUBSYSTEMS_TIMER_H

/**** INCLUDES ****/
#include <stddef.h>
#include <sys/types.h>
#include <stdbool.h>
#include <kernel/misc/spinlock.h>
#include <kernel/tasklet.h>

/**** DEFINITIONS ****/


/**** TYPES ****/


struct timer_device;
struct timer_event;

typedef tasklet_cb_t timer_expire_t;

/* Timer operations */
typedef struct timer_ops {
    void (*init)(struct timer_device *);
    void (*deinit)(struct timer_device *);
    void (*set_timer)(struct timer_device *, uint64_t);
    void (*stop)(struct timer_device *);
    uint64_t (*get_ticks_remaining)(struct timer_device *);
    uint64_t (*get_ticks_elapsed)(struct timer_device *);
} timer_ops_t;

/* Primary timer device. Each of these should be per-CPU. */
typedef struct timer_device {
    char *name;                     // Name of the device
    unsigned char priority;         // 0-255 priority. If the current timer device has less priority will use this one.
    
    // used to convert nanoseconds to ticks
    // standard formula: x = (y * freq.mult) / (1 << freq.shift)
    struct {
        uint64_t mult;  
        unsigned char shift;
    } freq;

    timer_ops_t *ops;

    // repeat handler
    tasklet_t repeat_tasklet;
    struct timer_event *pending_repeats;

    spinlock_t queue_lock;
    uint64_t total_ticks;             // Total amount of ticks, updated async.
    struct timer_event *queue;
    
    void *private;
} timer_device_t;

/* Individual timer */
typedef struct timer_event {
    struct timer_event *next;
    struct timer_event *prev;
    uint64_t absolute_expire; // in ticks
    tasklet_t tsklet; // stores timer name
    uint64_t intv; // in ticks
    bool repeat; // if repeating will be requeued as a tasklet
    bool active;
} timer_event_t;

/**** FUNCTIONS ****/

/**
 * @brief Create a timer device
 * @param name Name of the timer device
 * @param ops Operations
 * @param priority Priority of the timer device
 * @param private Timer-specific
 */
timer_device_t *timer_createDevice(char *name, timer_ops_t *ops, unsigned char priority, void *private);

/**
 * @brief Select the timer for this CPU
 * @param timer The timer to select for the CPU
 */
void timer_selectDevice(timer_device_t *device);

/**
 * @brief Timer IRQ callback
 */
void timer_irq();

/**
 * @brief Initialize a timer
 * @param timer The timer to initialize
 * @param expire Expire function
 * @param expire_ctx Expire context
 * @param ns Nanoseconds to expiration
 * @param repeat Whether timer should be repeated
 * @param name Optional timer name
 * @returns 0 on success
 */
int timer_init(timer_event_t *timer, timer_expire_t expire, void *expire_ctx, time_t ns, bool repeat, char *name);

/**
 * @brief Insert and start timer
 * @param timer The timer to insert
 */
int timer_insert(timer_event_t *timer);

/**
 * @brief Remove a timer entry
 * @param timer The timer to remove
 */
int timer_remove(timer_event_t *timer);


#endif
