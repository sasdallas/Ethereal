/**
 * @file libpolyhedron/semaphore/destroy.c
 * @brief sem_destroy
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
#include <stdlib.h>

int sem_destroy(sem_t *sem) {
    free(sem);
    return 0;
}