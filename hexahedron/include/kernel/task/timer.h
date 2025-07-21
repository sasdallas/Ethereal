/**
 * @file hexahedron/include/kernel/task/timer.h
 * @brief timer.h
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef KERNEL_TASK_TIMER_H
#define KERNEL_TASK_TIMER_H

/**** INCLUDES ****/
#include <time.h>
#include <stdint.h>

/**** TYPES ****/

struct process;

typedef struct process_timer {
    int which;                              // The type of timer we are
    struct timeval reset_value;             // Reset value for the timer
    struct timeval value;                   // Current value of the timer
    struct process *process;                // Process which has the timer

    unsigned long expire_seconds;           // Expire seconds
    unsigned long expire_subseconds;        // Expire subseconds
} process_timer_t;  


/**** FUNCTIONS ****/

/**
 * @brief Set and queue a timer for a process
 * @param process The process to queue the timer for
 * @param which The type of timer to queue
 * @param value The new value of the timer
 * @returns 0 on success
 */
int timer_set(struct process *process, int which, struct itimerval *value);

#endif