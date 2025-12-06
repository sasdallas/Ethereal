/**
 * @file hexahedron/misc/spinlock.c
 * @brief Spinlock implementation
 * 
 * This is used for safeguarding SMP memory accesses. It is an internal spinlock implementation.
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <kernel/misc/spinlock.h>
#include <kernel/mm/alloc.h>
#include <kernel/debug.h>
#include <kernel/arch/arch.h>
#include <stdatomic.h>
#include <assert.h>

/**
 * @brief Create a new spinlock
 * @param name Optional name parameter
 * @returns A new spinlock structure
 */
spinlock_t *spinlock_create(char *name) {
    spinlock_t *ret = kmalloc(sizeof(spinlock_t));
    ret->cpu = -1;
    ret->name = name;
    
    // Because atomics and its consequences have been a disaster for the human race,
    // we must use atomic_flag_clear to set this variable.
    // https://stackoverflow.com/questions/31526556/initializing-an-atomic-flag-in-a-mallocd-structure
    atomic_flag_clear(&(ret->lock));

    return ret;
}

/**
 * @brief Destroys a spinlock
 * @param spinlock Spinlock to destroy
 */
void spinlock_destroy(spinlock_t *spinlock) {
    kfree(spinlock); // TODO: Atomics?
}

/**
 * @brief Lock a spinlock
 * 
 * Will spin around until we acquire the lock
 */
void spinlock_acquire(spinlock_t *spinlock) {
    int state = hal_getInterruptState();
    hal_setInterruptState(HAL_INTERRUPTS_DISABLED);

    while (atomic_flag_test_and_set_explicit(&(spinlock->lock), memory_order_acquire)) {
        arch_pause_single();
    }

    spinlock->state = state;
    spinlock->cpu = arch_current_cpu();
}

/**
 * @brief Try to lock a spinlock
 * @param spinlock The spinlock to lock
 * @returns 1 on lock success
 */
int spinlock_tryAcquire(spinlock_t *spinlock) {
    int state = hal_getInterruptState();
    hal_setInterruptState(HAL_INTERRUPTS_DISABLED);

    if (atomic_flag_test_and_set_explicit(&(spinlock->lock), memory_order_acquire)) {
        hal_setInterruptState(state);
        return 0;
    }

    spinlock->state = state;
    spinlock->cpu = arch_current_cpu();
    return 1;
}

/**
 * @brief Release a spinlock
 */
void spinlock_release(spinlock_t *spinlock) {
    spinlock->cpu = -1;
    atomic_flag_clear_explicit(&(spinlock->lock), memory_order_release);
    hal_setInterruptState(spinlock->state);
}