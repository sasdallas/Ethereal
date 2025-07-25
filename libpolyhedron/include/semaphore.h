/**
 * @file libpolyhedron/include/semaphore.h
 * @brief Semaphores
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <sys/cheader.h>

_Begin_C_Header;

#ifndef _SEMAPHORE_H
#define _SEMAPHORE_H

/**** INCLUDES ****/
#include <fcntl.h>
#include <time.h>

/**** TYPES ****/
typedef struct {
    uint8_t pshared;                        // Shared between processes
    volatile unsigned int value;            // Value of the semaphore
} sem_t;

/**** DEFINITIONS ****/

#define SEM_FAILED      ((sem_t*)0)

/**** FUNCTIONS ****/

int    sem_close(sem_t *);
int    sem_destroy(sem_t *);
int    sem_getvalue(sem_t *, int *);
int    sem_init(sem_t *, int, unsigned int);
sem_t *sem_open(const char *, int, ...);
int    sem_post(sem_t *);
int    sem_trywait(sem_t *);
int     sem_timedwait(sem_t *sem, const struct timespec *abs_timeout);
int    sem_unlink(const char *);
int    sem_wait(sem_t *);

#endif

_End_C_Header;