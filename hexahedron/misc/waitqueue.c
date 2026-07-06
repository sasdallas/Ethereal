/**
 * @file hexahedron/misc/waitqueue.c
 * @brief Wait queue daemon
 * 
 * Wait queues are the successors to sleep queues
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#include <kernel/misc/waitqueue.h>
#include <kernel/task/process.h>
#include <kernel/processor_data.h>
#include <kernel/debug.h>
#include <assert.h>

#define LOCK_WAKE(n) __atomic_test_and_set(&(n)->wake_lock, __ATOMIC_ACQUIRE)
#define UNLOCK_WAKE(n) __atomic_clear(&(n)->wake_lock, __ATOMIC_RELEASE)

/**
 * @brief Wait queue add node
 * @param wq The wait queue to wait on
 * @param node The node to use (does not need initialization)
 * 
 * Doesn't prime you to sleep just yet
 */
void waitqueue_add(wait_queue_t *wq, wait_queue_node_t *n) {
    // Initialize the wait queue node
    n->next = NULL;
    n->thr = current_cpu->current_thread;
    n->ready = 0;

    // Acquire the wake lock to prevent anything else from waking this
    LOCK_WAKE(n);

    // Insert into waitqueue
    spinlock_acquire(&wq->lck);
    if (wq->tail == NULL) {
        wq->head = wq->tail = n;
    } else {
        wq->tail->next = n;
        wq->tail = n;
        n->next = NULL;
    }
    spinlock_release(&wq->lck);
}

/**
 * @brief Wait queue wait node
 * @param wq The wait queue to wait on
 * @param node The node to wait with
 * @param timeout The timeout to sleep for
 * @returns Error code or 0 on success
 */
int waitqueue_wait(wait_queue_t *wq, wait_queue_node_t *n, int timeout) {
    if (n->thr == NULL) {
        // NULL threads indicate that the kernel wants to sleep
        assert(timeout == -1 && "kernel sleeping on timeout not supported");

        while (__atomic_load_n(&n->ready, __ATOMIC_SEQ_CST) == false) {
            arch_pause_single();
        }

        return 0;
    }

    // Prepare to sleep
    if (timeout != -1) {
        sleep_time(timeout / 1000, timeout % 1000);
    } else {
        sleep_prepare();
    }

    // Acquire wq lock temporarily to check if we got signalled
    spinlock_acquire(&wq->lck);
    if (__atomic_load_n(&n->ready, __ATOMIC_SEQ_CST) == true) {
        spinlock_release(&wq->lck);
        sleep_exit();
        return 0;
    }
    
    UNLOCK_WAKE(n);
    spinlock_release(&wq->lck);

    // Nah go sleep
    int w = sleep_enter();

    // If another thread didn't wake us up we need this lock
    LOCK_WAKE(n);

    // Ignore w value if we were awoken
    if (__atomic_load_n(&n->ready, __ATOMIC_SEQ_CST) == true) {
        return 0;
    }

    // If we woke up
    if (w == WAKEUP_TIME) {
        return -ETIMEDOUT;
    } else if (w == WAKEUP_SIGNAL) {
        return -EINTR;
    } else {
        return 0;
    }
}

/**
 * @brief Remove from wait queue
 * @param wq The wait queue to remove from
 * @param n The node to remove
 * Must be called on exit of scope where the node is defined
 */
void waitqueue_remove(wait_queue_t *wq, wait_queue_node_t *n) {
    spinlock_acquire(&wq->lck);

    // TODO: this is O(n), maybe should use doubly-linked list?
    wait_queue_node_t *iter = wq->head;
    if (iter == n) {
        wq->head = iter->next;
        if (n == wq->tail) wq->tail = n->next;
    } else {
        while (iter) {
            if (iter->next == n) {
                break;
            }

            iter = iter->next;
        }

        assert(iter && "double waitqueue_remove");
        
        iter->next = n->next;
        if (n == wq->tail) wq->tail = iter;
    }

    spinlock_release(&wq->lck);
}

/**
 * @brief Signal a wait queue to wakeup some threads
 * @param wq The wait queue to signal
 * @param nthreads The number of threads to wakeup (0 = all threads) 
 */
int waitqueue_wakeup(wait_queue_t *wq, int nthreads) {
    int threads_awoken = 0;
    int threads_to_wake = nthreads ? nthreads : INT_MAX;
    spinlock_acquire(&wq->lck);

    wait_queue_node_t *n = wq->head;
    while (threads_to_wake && n) {
        if (__atomic_exchange_n(&n->ready, true, __ATOMIC_SEQ_CST) == false) {
            // We were the first to get this node
            threads_awoken++;
            threads_to_wake--;
        }

        if (!LOCK_WAKE(n)) {
            if (n->thr) sleep_wakeup(n->thr);
        }

        n = n->next;
    }

    spinlock_release(&wq->lck);
    return threads_awoken;
}
