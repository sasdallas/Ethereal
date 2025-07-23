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

// Sourced from reduceOS 1.x scheduler
#define PROCESS_KERNEL          0x01    // Process is a kernel-mode process 
#define PROCESS_STARTED         0x02    // Process started
#define PROCESS_RUNNING         0x04    // Process running
#define PROCESS_STOPPED         0x08    // Process stopped
#define PROCESS_SLEEPING        0x10    // Process sleeping
#define PROCESS_SUSPENDED       0x20    // Process suspended

// Priorities
#define PRIORITY_HIGH           3
#define PRIORITY_MED            2
#define PRIORITY_LOW            1

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
 * @brief Queue in a new thread
 * @param thread The thread to queue in
 * @returns 0 on success
 */
int scheduler_insertThread(thread_t *thread);

/**
 * @brief Remove a thread from the queue
 * @param thread The tread to remove
 * @returns 0 on success
 */
int scheduler_removeThread(thread_t *thread);

/**
 * @brief Reschedule the current thread
 * 
 * Whenever a thread hits 0 on its timeslice, it is automatically popped and
 * returned to the back of the list.
 */
void scheduler_reschedule();

/**
 * @brief Get the next thread to switch to
 * @returns A pointer to the next thread
 */
thread_t *scheduler_get();

/**
 * @brief Scheduler tick method, called every update
 */
int scheduler_update(uint64_t ticks);

#endif