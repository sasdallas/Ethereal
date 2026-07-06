/**
 * @file hexahedron/include/kernel/misc/waitqueue.h
 * @brief Wait queue implementation (successor to sleep queue)
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#ifndef KERNEL_MISC_WAITQUEUE_H
#define KERNEL_MISC_WAITQUEUE_H

/**** INCLUDES ****/
#include <stdint.h>
#include <kernel/misc/spinlock.h>
#include <stdbool.h>

/**** TYPES ****/ 

struct thread;

typedef struct wait_queue {
    spinlock_t lck;
    struct wait_queue_node *head;
    struct wait_queue_node *tail;
} wait_queue_t;

typedef struct wait_queue_node {
    struct wait_queue_node *next;
    struct thread *thr;
    unsigned char wake_lock; // not a spinlock, those mess with IRQs
    bool ready; // for early wakey
} wait_queue_node_t;

/**** MACROS ****/

#define WAIT_QUEUE_INITIALIZER { .lck = SPINLOCK_INITIALIZER, .head = NULL, .tail = NULL }

#define WAIT_QUEUE_INIT(wq) ({\
                                SPINLOCK_INIT(&(wq)->lck);\
                                (wq)->head = (wq)->tail = NULL;\
                            })

/**** FUNCTIONS ****/

/**
 * @brief Wait queue add node
 * @param wq The wait queue to wait on
 * @param node The node to use (does not need initialization)
 * 
 * Doesn't prime you to sleep just yet
 */
void waitqueue_add(wait_queue_t *wq, wait_queue_node_t *n);

/**
 * @brief Wait queue wait node
 * @param wq The wait queue to wait on
 * @param node The node to wait with
 * @param timeout The timeout to sleep for
 * @returns Error code or 0 on success
 */
int waitqueue_wait(wait_queue_t *wq, wait_queue_node_t *n, int timeout);

/**
 * @brief Remove from wait queue
 * @param wq The wait queue to remove from
 * @param n The node to remove
 * Must be called on exit of scope where the node is defined
 */
void waitqueue_remove(wait_queue_t *wq, wait_queue_node_t *n);

/**
 * @brief Signal a wait queue to wakeup some threads
 * @param wq The wait queue to signal
 * @param nthreads The number of threads to wakeup (0 = all threads) 
 */
int waitqueue_wakeup(wait_queue_t *wq, int nthreads);

#endif
