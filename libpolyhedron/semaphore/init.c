/**
 * @file libpolyhedron/semaphore/init.c
 * @brief sem_init
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

int sem_init(sem_t *sem, int pshared, unsigned int value) {
    sem->pshared = pshared;
    sem->value = value;
    return 0;
}