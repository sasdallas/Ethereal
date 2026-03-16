/**
 * @file hexahedron/include/kernel/lock/rwsem.h
 * @brief Read-write semaphore
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef KERNEL_LOCK_RWSEM_H
#define KERNEL_LOCK_RWSEM_H

/**** INCLUDES ****/
#include <kernel/misc/mutex.h>
#include <kernel/task/sleep.h>
#include <stddef.h>
#include <stdatomic.h>

/**** TYPES ****/

typedef struct rwsem {
    volatile int state; // bits 0-30 are readers count, bit 31 is whether writers are active
    volatile int writers_waiting;
    sleep_queue_t reader_queue;
    sleep_queue_t writer_queue;
} rwsem_t;

/**** MACROS ****/

#define RWSEM_INITIALIZER { .state = 0, .writers_waiting = 0, .reader_queue = SLEEP_QUEUE_INITIALIZER, .writer_queue = SLEEP_QUEUE_INITIALIZER }
#define RWSEM_INIT(s) ({ atomic_store(&(s)->state, 0); (s)->writers_waiting = 0; SLEEP_QUEUE_INIT(&(s)->reader_queue); SLEEP_QUEUE_INIT(&(s)->writer_queue); })

/**** FUNCTIONS ****/

/**
 * @brief Start a read on a read/write semaphore
 * @param rwsem The semaphore to read
 * @returns 0 on success. Note that it doesn't return @c sleep_enter failures
 */
int rwsem_startRead(rwsem_t *rwsem);

/**
 * @brief Finish reading on read/write semaphore
 * @param rwsem The semaphore to finish reading on
 */
void rwsem_finishRead(rwsem_t *rwsem);

/**
 * @brief Start a write on a read/write semaphore
 * @param rwsem The semaphore to start a write on
 * @returns 0 on success. Note that it doesn't return @c sleep_enter failures
 */
int rwsem_startWrite(rwsem_t *rwsem);

/**
 * @brief Finish a write on a read/write semaphore
 * @param rwsem The semaphore to finish the write on
 */
void rwsem_finishWrite(rwsem_t *rwsem);

#endif