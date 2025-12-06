/**
 * @file hexahedron/misc/semaphore.c
 * @brief Semaphore synchronization system 
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <kernel/misc/semaphore.h>
#include <kernel/panic.h>
#include <kernel/mm/alloc.h>

/**
 * @brief Initialize and create a semaphore
 * @param name          Optional semaphore name (for debugging)
 * @param value         The initialization value of the semaphore
 * @param max_value     The maximum value of the semaphore (afterwards, signal will wait)
 */
semaphore_t *semaphore_create(char *name, int value, int max_value) {
    semaphore_t *output = kmalloc(sizeof(semaphore_t));
    output->lock = spinlock_create("sem_lock");
    output->value = value;
    output->max_value = max_value;
    output->name = name;
    return output;
}

/**
 * @brief Wait on the semaphore
 * @param semaphore     The semaphore to use
 * @param items         The amount of items to take from the semaphore
 * @returns Items taken
 */
int semaphore_wait(semaphore_t *semaphore, int items) {
    // Lock the semaphore
    spinlock_acquire(semaphore->lock);

    int items_taken = 0;

    // Wait won't fault on not enough items taken
    if (semaphore->value > 0) {
        if (semaphore->value > items) items_taken = items;
        else items_taken = semaphore->value;
        semaphore->value -= items_taken;
    } else {
        // Wait how do we do that again
        // Yeah I don't know what I am doing
        kernel_panic_extended(KERNEL_BAD_ARGUMENT_ERROR, "semaphore", "*** Semaphore underflowed max value. No thread implementation is available\n"); 
    }

    // Release the semaphore
    spinlock_release(semaphore->lock);

    return items_taken;
}

/**
 * @brief Signal to the semaphore
 * @param semaphore     The semaphore to use
 * @param items         The amount of items to add to the semaphore
 * @returns Items added
 */
int semaphore_signal(semaphore_t *semaphore, int items) {
    // Lock the semaphore
    spinlock_acquire(semaphore->lock);

    int items_added = 0;

    // Okay, do we need to wait?
    if (semaphore->max_value && semaphore->value >= semaphore->max_value) {
        // Wait how do we do that again
        // Yeah I don't know what I am doing
        kernel_panic_extended(KERNEL_BAD_ARGUMENT_ERROR, "semaphore", "*** Semaphore exceeded max_value. No thread implementation is available.\n"); 
    } else {
        // Just add to semaphore. Make sure to limit though to prevent chaos
        if (semaphore->value + items > semaphore->max_value) {
            items_added = semaphore->max_value - semaphore->value;
        } else {
            items_added = items;
        }

        semaphore->value += items_added;
    }

    // Release the semaphore
    spinlock_release(semaphore->lock);

    return items_added;
}

/**
 * @brief Get the semaphore's items
 * @param semaphore     The semaphore to use
 * @returns The amount of items in the semaphore
 */
int semaphore_getItems(semaphore_t *semaphore) {
    return semaphore->value;
}

/**
 * @brief Destroy the semaphore
 * @param semaphore     The semaphore to use
 */
void semaphore_destroy(semaphore_t *semaphore) {
    // !!!: Free semaphore->name?
    kfree(semaphore->lock);
    kfree(semaphore);
}