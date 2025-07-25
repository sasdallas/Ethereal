/**
 * @file libpolyhedron/semaphore/post.c
 * @brief sem_post
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
#include <stdatomic.h>

int sem_post(sem_t *sem) {
    if (!sem) {
        errno = EINVAL;
        return -1;   
    }

    atomic_fetch_add_explicit(&sem->value, 1, memory_order_release);
    return 0;
}