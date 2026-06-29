/**
 * @file hexahedron/lock/rwsem.c
 * @brief Read-write semaphores
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/lock/rwsem.h>

#define RWSEM_WRITER_ACTIVE(s) (!!((s) & (1u << 31)))
#define RWSEM_READERS_ACTIVE(s) ((s) & ~(1u << 31))

/**
 * @brief Start a read on a read/write semaphore
 * @param rwsem The semaphore to read
 */
int rwsem_startRead(rwsem_t *rwsem) {
    while (1) {
        int old = atomic_load(&rwsem->state);
        if (RWSEM_WRITER_ACTIVE(old) || atomic_load(&rwsem->writers_waiting) > 0) {
            // There's a writer active right now
            sleep_inQueue(&rwsem->reader_queue);
            sleep_enter();
            continue;
        }

        if (atomic_compare_exchange_weak(&rwsem->state, &old, old+1)) break;
    }

    return 0;
}

/**
 * @brief Finish reading on read/write semaphore
 * @param rwsem The semaphore to finish reading on
 */
void rwsem_finishRead(rwsem_t *rwsem) {
    while (1) {
        int old = atomic_load(&rwsem->state);
        if (atomic_compare_exchange_weak(&rwsem->state, &old, old-1)) break;
    }

    if (RWSEM_READERS_ACTIVE(atomic_load(&rwsem->state)) == 0 && atomic_load(&rwsem->writers_waiting) != 0) {
        sleep_wakeupQueue(&rwsem->writer_queue, 1);
    }
}

/**
 * @brief Start a write on a read/write semaphore
 * @param rwsem The semaphore to start a write on
 */
int rwsem_startWrite(rwsem_t *rwsem) {
    atomic_fetch_add(&rwsem->writers_waiting, 1);
    while (1) {
        int old = atomic_load(&rwsem->state);
        if (RWSEM_WRITER_ACTIVE(old) || RWSEM_READERS_ACTIVE(old) > 0) {
            sleep_inQueue(&rwsem->writer_queue);
            sleep_enter();
            continue;
        }

        if (atomic_compare_exchange_weak(&rwsem->state, &old, old | (1u << 31))) break;
    }

    atomic_fetch_sub(&rwsem->writers_waiting, 1);
    return 0;
}

/**
 * @brief Finish a write on a read/write semaphore
 * @param rwsem The semaphore to finish the write on
 */
void rwsem_finishWrite(rwsem_t *rwsem) {
    // Clear writer bit
    atomic_fetch_and(&rwsem->state, ~(1u << 31));

    if (atomic_load(&rwsem->writers_waiting)) {
        sleep_wakeupQueue(&rwsem->writer_queue, 1);
    } else {
        sleep_wakeupQueue(&rwsem->reader_queue, -1);
    }
}
