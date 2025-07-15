/**
 * @file libpolyhedron/pthread/cond.c
 * @brief pthread_cond
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 * 
 * @note Thank you to @Bananymous for the idea of using a block list :D
 */

#include <pthread.h>
#include <string.h>

int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *condattr) {
    if (condattr) {
        memcpy(&cond->attr, condattr, sizeof(pthread_condattr_t));
    } else {
        cond->attr.clock = CLOCK_REALTIME;
        cond->attr.shared = PTHREAD_PROCESS_PRIVATE;
    }

    cond->lock = PTHREAD_SPIN_INITIALIZER;
    cond->blk = NULL;

    return 0;
}

int pthread_cond_destroy(pthread_cond_t *cond) {
    return 0;
}

int pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime) {
    // TODO: testcancel
    // TODO: abstime (desperately need this)

    // Add ourselves to the block list
    pthread_spin_lock(&cond->lock);
    __pthread_condblocker_t blocker = { 
        .next = cond->blk,
        .prev = NULL,
        .signalled = 0,
    };

    // Update head if needed
    if (cond->blk) cond->blk->prev = &blocker;

    cond->blk = &blocker;
    pthread_spin_unlock(&cond->lock);

    // Release the mutex
    pthread_mutex_unlock(mutex);

    // Check for atomic
    while (__atomic_load_n(&blocker.signalled, __ATOMIC_SEQ_CST) == 0) {
        // TODO: Check timeout with clock_gettime
        sched_yield();
    }

    // We were signalled, reorder the list a bit
    pthread_spin_lock(&cond->lock);
    if (cond->blk == &blocker) {
        // We are still the head
        cond->blk = blocker.next;
        if (blocker.next) blocker.next->prev = NULL;
    } else {
        // We are not the head, reorder the linked list.
        if (blocker.prev) blocker.prev->next = blocker.next;
        if (blocker.next) blocker.next->prev = blocker.prev;
    }
    pthread_spin_unlock(&cond->lock);

    pthread_mutex_lock(mutex);
    return 0;
}

int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex) {
    return pthread_cond_timedwait(cond, mutex, NULL);
}

int pthread_cond_signal(pthread_cond_t *cond) {
    // Signal the head
    pthread_spin_lock(&cond->lock);
    if (cond->blk) __atomic_store_n(&cond->blk->signalled, 1, __ATOMIC_SEQ_CST);
    pthread_spin_unlock(&cond->lock);
    return 0;
}

int pthread_cond_broadcast(pthread_cond_t *cond) {
    pthread_spin_lock(&cond->lock);

    // Signal all of them
    __pthread_condblocker_t *blk = cond->blk;
    while (blk) {
        __atomic_store_n(&blk->signalled, 1, __ATOMIC_SEQ_CST);
        blk = blk->next;
    }

    pthread_spin_unlock(&cond->lock);
    return 0;
}