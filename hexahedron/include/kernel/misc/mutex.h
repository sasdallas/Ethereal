/**
 * @file hexahedron/include/kernel/misc/mutex.h
 * @brief Mutex system for kernel
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef KERNEL_MISC_MUTEX_H
#define KERNEL_MISC_MUTEX_H

/**** INCLUDES ****/
#include <stdint.h>
#include <stdatomic.h>
#include <kernel/task/sleep.h>
#include <sys/types.h>

/**** TYPES ****/

typedef struct mutex {
    char *name;             // Optional mutex name
    volatile pid_t lock;    // Mutex lock
    sleep_queue_t *queue;   // Sleep queue
} mutex_t;

/**** MACROS ****/

// AVOID
#define MUTEX_DEFINE_LOCAL(n) static sleep_queue_t __queue_##n = { 0 }; \
                                    static mutex_t n = { .lock = -1, .name = NULL, .queue = &__queue_##n};

/**** FUNCTIONS ****/

/**
 * @brief Create a new mutex
 * @param name Optional mutex name
 */
mutex_t *mutex_create(char *name);

/**
 * @brief Acquire the mutex
 * @param mutex The mutex to acquire
 */
void mutex_acquire(mutex_t *mutex);

/**
 * @brief Try to acquire the mutex
 * @param mutex The mutex to try to acquire
 * @returns 1 on successful acquire
 */
int mutex_tryAcquire(mutex_t *mutex);

/**
 * @brief Release the mutex
 * @param mutex The mutex to release
 */
void mutex_release(mutex_t *mutex);

/**
 * @brief Destroy a mutex
 * @param mutex The mutex to destroy
 */
void mutex_destroy(mutex_t *mutex);

#endif