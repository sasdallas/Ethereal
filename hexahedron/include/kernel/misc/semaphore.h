/**
 * @file hexahedron/include/kernel/misc/semaphore.h
 * @brief Header for the semaphore interface (binary semaphores) 
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_MISC_SEMAPHORE_H
#define KERNEL_MISC_SEMAPHORE_H

/**** INCLUDES ****/
#include <stdint.h>
#include <kernel/misc/spinlock.h>

/**** TYPES ****/
typedef struct _semaphore {
    spinlock_t *lock;           // TODO: This would benefit from a shorter lock
    char *name;                 // Optional name for debugging
    volatile int value;         // Current value
    volatile int max_value;     // Maximum value (signal will wait if exceeded)

    // TODO: When threading is implemented, thread lists will be put here.
} semaphore_t;

/**** FUNCTIONS ****/

/**
 * @brief Initialize and create a semaphore
 * @param name          Optional semaphore name (for debugging)
 * @param value         The initialization value of the semaphore
 * @param max_value     The maximum value of the semaphore (afterwards, signal will wait)
 */
semaphore_t *semaphore_create(char *name, int value, int max_value);

/**
 * @brief Wait on the semaphore
 * @param semaphore     The semaphore to use
 * @param items         The amount of items to take from the semaphore
 * @returns Items taken
 */
int semaphore_wait(semaphore_t *semaphore, int items);

/**
 * @brief Signal to the semaphore
 * @param semaphore     The semaphore to use
 * @param items         The amount of items to add to the semaphore
 * @returns Items added
 */
int semaphore_signal(semaphore_t *semaphore, int items);

/**
 * @brief Get the semaphore's items
 * @param semaphore     The semaphore to use
 * @returns The amount of items in the semaphore
 */
int semaphore_getItems(semaphore_t *semaphore);

/**
 * @brief Destroy the semaphore
 * @param semaphore     The semaphore to use
 */
void semaphore_destroy(semaphore_t *semaphore);

#endif