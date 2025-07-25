/**
 * @file libpolyhedron/semaphore/wait.c
 * @brief sem_timedwait and sem_wait 
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <semaphore.h>
#include <errno.h>
#include <sched.h>

int sem_timedwait(sem_t *sem, const struct timespec *abs_timeout) {
    if (!sem) {
        errno = EINVAL;
        return -1;
    }

    while (1) {
        int current_value = __atomic_load_n(&sem->value, __ATOMIC_SEQ_CST);

        if (current_value > 0) {
            if (__atomic_compare_exchange_n(&sem->value, &current_value, current_value - 1, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
                break;
            }
        } else {
            if (abs_timeout) {
                struct timeval now;
                gettimeofday(&now, NULL);

                if (now.tv_sec > abs_timeout->tv_sec || 
                    (now.tv_sec == abs_timeout->tv_sec && now.tv_usec >= abs_timeout->tv_nsec)) {
                    errno = ETIMEDOUT;
                    return -1;
                }
            }

            sched_yield();
        }
    }

    return 0;
}


int sem_wait(sem_t *sem) {
    return sem_timedwait(sem, NULL);
}

int sem_trywait(sem_t *sem) {
    if (!sem) {
        errno = EINVAL;
        return -1;
    }

    // Try to get it
    unsigned int value = __atomic_load_n(&sem->value, __ATOMIC_SEQ_CST);
    if (value) {
        __atomic_compare_exchange_n(&sem->value, &value, value - 1, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
        return 0;
    }

    // We didn't get the semaphore
    errno = EAGAIN;
    return -1;
}