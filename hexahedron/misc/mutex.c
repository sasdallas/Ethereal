/**
 * @file hexahedron/misc/mutex.c
 * @brief Kernel mutex implementation
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/misc/mutex.h>
#include <kernel/mm/alloc.h>
#include <kernel/task/process.h>
#include <string.h>

/**
 * @brief Create a new mutex
 * @param name Optional mutex name
 */
mutex_t *mutex_create(char *name) {
    mutex_t *m = kzalloc(sizeof(mutex_t));
    __atomic_store_n(&(m->lock), -1, __ATOMIC_SEQ_CST);

    m->name = name;
    memset(&m->queue, 0, sizeof(sleep_queue_t));

    return m;
}

/**
 * @brief Acquire the mutex
 * @param mutex The mutex to acquire
 */
void mutex_acquire(mutex_t *mutex) {
    pid_t expect = (pid_t)-1;
    pid_t want = (current_cpu->current_thread ? current_cpu->current_thread->tid : 0); // 0 is the kernel reserved TID 
    while (!__atomic_compare_exchange_n(&mutex->lock, &expect, want, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
        if (current_cpu->current_thread) {
            sleep_inQueue(&mutex->queue);
            sleep_enter();
        } else {
            arch_pause_single();
        }

        expect = (pid_t)-1;
    }
}

/**
 * @brief Try to acquire the mutex
 * @param mutex The mutex to try to acquire
 * @returns 1 on successful acquire
 */
int mutex_tryAcquire(mutex_t *mutex) {
    pid_t expect = (pid_t)-1;
    pid_t want = (current_cpu->current_thread ? current_cpu->current_thread->tid : 0); // 0 is the kernel reserved TID 
    return __atomic_compare_exchange_n(&mutex->lock, &expect, want, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

/**
 * @brief Release the mutex
 * @param mutex The mutex to release
 */
void mutex_release(mutex_t *mutex) {
    __atomic_store_n(&mutex->lock, -1, __ATOMIC_SEQ_CST);
    sleep_wakeupQueue(&mutex->queue, 1);
}

/**
 * @brief Destroy a mutex
 * @param mutex The mutex to destroy
 */
void mutex_destroy(mutex_t *mutex) {
    kfree(mutex);
}