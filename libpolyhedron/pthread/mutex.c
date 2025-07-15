/**
 * @file libpolyhedron/pthread/mutex.c
 * @brief pthread mutexes
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
#include <string.h>

int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr) {
    if (mutexattr) {
        memcpy(&mutex->attr, mutexattr, sizeof(pthread_mutexattr_t));
    } else {
        mutex->attr.type = PTHREAD_MUTEX_DEFAULT;
        mutex->attr.pshared = PTHREAD_PROCESS_PRIVATE;
        mutex->attr.protocol = PTHREAD_PRIO_NONE;
        mutex->attr.robust = PTHREAD_MUTEX_STALLED;
    }
    
    mutex->lock = PTHREAD_SPIN_INITIALIZER;
    return 0;
}

int pthread_mutex_destroy(pthread_mutex_t *mutex) {
    return 0;
}

int pthread_mutex_lock(pthread_mutex_t *mutex) {
    return pthread_spin_lock(&mutex->lock);
}

int pthread_mutex_trylock(pthread_mutex_t *mutex) {
    return pthread_spin_trylock(&mutex->lock);
}

int pthread_mutex_unlock(pthread_mutex_t *mutex) {
    return pthread_spin_unlock(&mutex->lock);
}

