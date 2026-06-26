/**
 * @file hexahedron/include/kernel/task/scheduler.h
 * @brief Scheduler header
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_TASK_SCHEDULER_H
#define KERNEL_TASK_SCHEDULER_H

/**** INCLUDES ****/
#include <stdint.h>
#include <structs/list.h>

/**** DEFINITIONS ****/

// Priorities
#define PRIORITY_HIGH           3
#define PRIORITY_MED            2
#define PRIORITY_LOW            1

#define SCHEDULER_STATE_INACTIVE    0
#define SCHEDULER_STATE_IDLE        1
#define SCHEDULER_STATE_ACTIVE      2

/**** VARIABLES ****/

/**
 * @brief Time slices for different priorities
 */
extern time_t scheduler_timeslices[];

/**** FUNCTIONS ****/

/**
 * @brief Initialize the scheduler
 */
void scheduler_init();

/**
 * @brief Initilaize the scheduler for a CPU
 */
void scheduler_initCPU();

/**
 * @brief Queue in a new thread
 * @param thread The thread to queue in
 * @returns 0 on success
 */
int scheduler_insertThread(thread_t *thread);

/**
 * @brief Yield a thread
 */
void scheduler_yield(thread_t *old);

/**
 * @brief Get the next thread to switch to
 * @returns A pointer to the next thread
 */
thread_t *scheduler_get();

#endif