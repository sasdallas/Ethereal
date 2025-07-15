/**
 * @file libpolyhedron/pthread/spinlock.c
 * @brief pthread_spinlock_*
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <pthread.h>
#include <errno.h>
#include <sched.h>

int pthread_spin_lock(pthread_spinlock_t *spinlock) {
    while (__sync_lock_test_and_set(spinlock, 1)) sched_yield();
    return 0;
}

int pthread_spin_trylock(pthread_spinlock_t *spinlock) {
    if (__sync_lock_test_and_set(spinlock, 1)) return EBUSY;
    return 0;
}

int pthread_spin_unlock(pthread_spinlock_t *spinlock) {
    __sync_lock_release(spinlock);
    return 0;
}
