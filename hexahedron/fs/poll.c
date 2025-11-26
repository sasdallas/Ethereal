/**
 * @file hexahedron/fs/poll.c
 * @brief Poll subsystem
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/fs/poll.h>
#include <kernel/mm/alloc.h>
#include <kernel/misc/util.h>
#include <kernel/panic.h>
#include <assert.h>
#include <errno.h>


/* UNCOMMENT TO ENABLE DEBUG */
// #define POLL_DEBUG 1

/* Log method */
#define LOG(status, ...) dprintf_module(status, "FS:POLL", __VA_ARGS__)

/**
 * @brief Create and initialize a waiter
 * @param thr The thread to use
 * @param nevents Number of events waiting on
 */
poll_waiter_t *poll_createWaiter(struct thread *thr, size_t nevents) {
    poll_waiter_t *w = kzalloc(sizeof(poll_waiter_t));
    w->thr = thr;
    w->nevents = nevents;
    w->i = 0;
    w->events =  kzalloc(sizeof(poll_event_t*) * nevents);
    w->dead = false;
    w->result = NULL;
    refcount_init(&w->refs, 1);
    spinlock_acquire(&w->lock);
    return w;
}

/**
 * @brief Add an interest on an event
 * @param waiter The waiter to add the interest for
 * @param event The event to wait on
 * @param events The events being waited on
 */
int poll_add(poll_waiter_t *waiter, poll_event_t *event, poll_events_t events) {
    spinlock_acquire(&event->lock);
    poll_waiter_node_t *n = kmalloc(sizeof(poll_waiter_node_t)); // TODO: cache this
    n->waiter = waiter;
    n->events = events;
    n->next = event->h;
    n->prev = NULL;
    refcount_inc(&waiter->refs);
    if (event->h) event->h->prev = n;
    event->h = n;
    assert(waiter->i < waiter->nevents); // If this fails more events were added on a waiter than the waiter was allocated for. Dumbass.
    waiter->events[waiter->i++] = event;
    spinlock_release(&event->lock);
    return 0;
}

/**
 * @brief Enter waiting
 * @param waiter The waiter to wait on
 * @param timeout The timeout (or -1 for infinite waiting)
 * @returns Error code (EINTR or ETIMEDOUT)
 */
int poll_wait(poll_waiter_t *waiter, int timeout) {
    spinlock_acquire(&waiter->result_lock);

    if (waiter->result) {
        spinlock_release(&waiter->result_lock);
        spinlock_release(&waiter->lock);
        return 0; // Events were given
    }

    // Prepare
    if (timeout != -1) {
        sleep_time(timeout / 1000, timeout % 1000);
    } else {
        sleep_prepare();
    }

    // NOW release the locks and result lock
    spinlock_release(&waiter->lock);
    spinlock_release(&waiter->result_lock);
    int w = sleep_enter();
    __atomic_store_n(&waiter->dead, true, __ATOMIC_SEQ_CST);

    int r = 0;
    if (w == WAKEUP_SIGNAL) {
        r = -EINTR;
    }

    if (w == WAKEUP_TIME) {
        r = -ETIMEDOUT;
    }

    return r;
}

/**
 * @brief Signal any events
 * @param event The event to signal
 * @param events The events to signal
 */
void poll_signal(poll_event_t *event, poll_events_t events) {
    spinlock_acquire(&event->lock);

    poll_waiter_node_t *wn = event->h;
    
    if (!wn) {
    #ifdef POLL_DEBUG
        LOG(DEBUG, "poll_signal had no events\n");
    #endif

        spinlock_release(&event->lock);
        return;
    }


    while (wn) {
        
        if (wn->waiter->dead) {
        #ifdef POLL_DEBUG
            LOG(DEBUG, "Dead waiter %p\n", wn->waiter);
        #endif

            poll_waiter_node_t *n = wn->next;
            if (n) n->prev = wn->prev;
            if (wn->prev) wn->prev->next = n;
            if (wn == event->h) event->h = wn->next;

            poll_destroyWaiter(wn->waiter);
            kfree(wn);

            wn = n;
            continue;
        }

        if (wn->events & events || (events & POLLHUP) || (events & POLLERR)) {
            // !!!: Waste
            poll_result_t *r = kmalloc(sizeof(poll_result_t));
            r->revents = (wn->events | POLLHUP | POLLERR) & events;
            r->ev = event;

            // Push the result
            spinlock_acquire(&wn->waiter->result_lock);
            r->next = wn->waiter->result;
            wn->waiter->result = r;
            spinlock_release(&wn->waiter->result_lock);

        #ifdef POLL_DEBUG
            LOG(DEBUG, "Triggering wakeup on waiter %p\n", wn->waiter);
        #endif
            sleep_wakeup(wn->waiter->thr);

            
            // Remove the waiter
        #ifdef POLL_DEBUG
            LOG(DEBUG, "Result appended for waiter %p\n", wn->waiter);
        #endif

            poll_destroyWaiter(wn->waiter);
            poll_waiter_node_t *n = wn->next;
            if (n) n->prev = wn->prev;
            if (wn->prev) wn->prev->next = n;
            if (wn == event->h) event->h = wn->next;
            kfree(wn);
            wn = n;
            continue;
        }

        wn = wn->next;
    }

    spinlock_release(&event->lock);
}

/**
 * @brief Exit a poll
 * @param waiter The waiter
 */
void poll_exit(poll_waiter_t *waiter) {
    __atomic_store_n(&waiter->dead, true, __ATOMIC_SEQ_CST);
    for (size_t i = 0; i < waiter->i; i++) {
        // this is to pull the waiter object out of the event object and destroy it
        poll_event_t *event = waiter->events[i];
        spinlock_acquire(&event->lock);
        
        poll_waiter_node_t *wn = waiter->events[i]->h;
        while (wn) {
            if (wn->waiter == waiter) {
                // Found it!
                poll_waiter_node_t *n = wn->next;
                if (n) n->prev = wn->prev;
                if (wn->prev) wn->prev->next = n;
                if (wn == event->h) event->h = wn->next;
                kfree(wn);
                refcount_dec(&waiter->refs);
                
                break;
            }

            wn = wn->next;
        }

        spinlock_release(&event->lock);
    }

    
}

/**
 * @brief Cleanup a waiter structure
 * Remember to call @c poll_exit
 */
void poll_destroyWaiter(poll_waiter_t *waiter) {
    assert(waiter->refs >= 1);
    refcount_dec(&waiter->refs);
    if (!waiter->refs) {
    #ifdef POLL_DEBUG
        LOG(DEBUG, "Destroying waiter %p\n", waiter);
        #endif

        poll_result_t *r = waiter->result;
        while (r) {
            poll_result_t *n = r->next;
            kfree(r);
            r = n;
        }

        kfree(waiter->events);
        kfree(waiter);
    }
}
