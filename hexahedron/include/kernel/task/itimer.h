/**
 * @file hexahedron/include/kernel/task/itimer.h
 * @brief itimer mechanism
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#ifndef KERNEL_TASK_ITIMER_H
#define KERNEL_TASK_ITIMER_H

/**** INCLUDES ****/
#include <stdint.h>
#include <stdatomic.h>
#include <kernel/subsystems/timer.h>
#include <kernel/misc/mutex.h>

/**** DEFINITIONS ****/

/**** TYPES ****/

typedef struct itimer {
    struct process *proc;
    timer_event_t timer;
    time_t ns_intv; // reset interval
    int signal;
    mutex_t lock;
} itimer_t;

/**** FUNCTIONS ****/

/**
 * @brief Initialize an itimer for a process
 * @param itimer The itimer to initialize
 * @param proc The process to signal
 * @param signal Signal to deliver upon timer
 */
void itimer_init(itimer_t *itimer, struct process *proc, int signal);

/**
 * @brief Set itimer
 * @param itimer The itimer to set
 * @param it_value Input is what to set, output is what was set
 * @param it_interval Ditto
 */
void itimer_set(itimer_t *itimer, struct timeval *it_value, struct timeval *it_interval);

/**
 * @brief Get itimer
 * @param itimer The itimer to get
 * @param it_value The value to get the itimer for
 * @param it_interval Repeat value
 */
void itimer_get(itimer_t *itimer, struct timeval *it_value, struct timeval *it_interval);

/**
 * @brief Destroy itimer
 * @param itimer The itimer to destroy
 */
static inline void itimer_destroy(itimer_t *itimer) {
    __atomic_store_n(&itimer->ns_intv, 0, __ATOMIC_RELEASE);
    timer_remove(&itimer->timer, NULL);
}

#endif
