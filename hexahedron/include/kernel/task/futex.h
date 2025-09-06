/**
 * @file hexahedron/include/kernel/task/futex.h
 * @brief Futex implementation
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef KERNEL_TASK_FUTEX_H
#define KERNEL_TASK_FUTEX_H

/**** INCLUDES ****/
#include <stdint.h>
#include <kernel/task/thread.h>
#include <kernel/task/sleep.h>
#include <sys/time.h>

/**** DEFINITIONS ****/

/**** TYPES ****/

typedef struct futex {
    sleep_queue_t *queue;           // Sleep queue

} futex_t;

/**** FUNCTIONS ****/

/**
 * @brief Wait on a futex
 * @param pointer Pointer to the futex variable
 * @param value The vaue to look for
 * @param time Time to wait on
 */
int futex_wait(int *pointer, int val, const struct timespec *time);

/**
 * @brief Wake up a futex
 * @param pointer Pointer to the futex
 */
int futex_wakeup(int *pointer);

/**
 * @brief Initialize futexes
 */
void futex_init();

#endif