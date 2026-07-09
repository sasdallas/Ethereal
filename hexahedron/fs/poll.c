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
#include <kernel/mm/slab.h>
#include <kernel/misc/util.h>
#include <kernel/task/process.h>
#include <kernel/panic.h>
#include <kernel/init.h>
#include <assert.h>
#include <errno.h>

/* UNCOMMENT TO ENABLE DEBUG */
// #define POLL_DEBUG 1

/* Log method */
#define LOG(status, ...) dprintf_module(status, "FS:POLL", __VA_ARGS__)

slab_cache_t *waiter_cache = NULL;
slab_cache_t *waiter_node_cache = NULL;

static void poll_freeWaiter(poll_waiter_t *w);

#define LOCK_WAITER(n) __atomic_test_and_set(&(n)->lock, __ATOMIC_ACQUIRE)
#define UNLOCK_WAITER(n) __atomic_clear(&(n)->lock, __ATOMIC_RELEASE)
#define MATCH_EVENTS(e1,e2) ((e1) & (e2) || ((e2) & (POLLHUP | POLLERR)))
#define HOLD_WAITER(n) refcount_inc(&(n)->refs)
#define RELEASE_WAITER(n) if (refcount_dec(&(n)->refs) == 1) poll_freeWaiter((n))



/**
 * @brief Create and initialize a waiter
 * @param thr The thread to use
 * @param nevents Number of events waiting on
 */
poll_waiter_t *poll_createWaiter(struct thread *thr, size_t nevents) {
    poll_waiter_t *w = slab_allocate(waiter_cache);
    assert(w);
    LOCK_WAITER(w);
    refcount_init(&w->refs, 1);
    w->thr = current_cpu->current_thread;
    w->nevents = nevents;
    w->events = kmalloc(sizeof(poll_event_t*) * nevents);
    w->ready = false;
    w->i = 0;
    return w;
}

/**
 * @brief Add an interest on an event
 * @param waiter The waiter to add the interest for
 * @param event The event to wait on
 * @param events The events being waited on
 */
int poll_add(poll_waiter_t *waiter, poll_event_t *event, poll_events_t events) {
    if (event->checker) {
        poll_events_t result = event->checker(event);
    
        if (MATCH_EVENTS(events,result)) {
            waiter->ready = true;
            return 0;
        }
    }

    poll_waiter_node_t *n = slab_allocate(waiter_node_cache);
    n->waiter = waiter;
    n->events = events;
    n->next = event->h;
    n->prev = NULL;

    spinlock_acquire(&event->lock);
    if (event->h) event->h->prev = n;
    event->h = n;
    assert(waiter->i < waiter->nevents);
    waiter->events[waiter->i++] = event;
    HOLD_WAITER(waiter);
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
    if (waiter->thr == NULL) {
        assert(timeout == -1 && "kernel sleeping on timeout not supported");
        while (__atomic_load_n(&waiter->ready, __ATOMIC_SEQ_CST) == false) {
            arch_pause_single();
        }

        return 0;   
    }

    if (timeout != -1) {
        sleep_time(timeout/1000, timeout%1000);
    } else {
        sleep_prepare();
    }

    // Release the lock right here
    UNLOCK_WAITER(waiter);

    // Check if we got signalled
    if (__atomic_load_n(&waiter->ready, __ATOMIC_SEQ_CST)) {
        sleep_exit();
        return 0;
    }

    int w = sleep_enter();

    // If another thread didnt wake us we need this lock
    LOCK_WAITER(waiter);

    // if we got awoken, w doesn't matter
    if (__atomic_load_n(&waiter->ready, __ATOMIC_SEQ_CST) == true) {
        return 0;
    }

    if (w == WAKEUP_TIME) {
        return -ETIMEDOUT;
    } else if (w == WAKEUP_SIGNAL) {
        return -EINTR;
    } else {
        return 0;
    }
}

/**
 * @brief Signal any events
 * @param event The event to signal
 * @param events The events to signal
 */
void poll_signal(poll_event_t *event, poll_events_t events) {
    spinlock_acquire(&event->lock);

    poll_waiter_node_t *wn = event->h;
    while (wn) {
        if (MATCH_EVENTS(wn->events, events)) {
            // we can try to wake this guy up
            __atomic_store_n(&wn->waiter->ready, true, __ATOMIC_SEQ_CST);
            
            if (!LOCK_WAITER(wn->waiter)) {
                if (wn->waiter->thr) {
                    sleep_wakeup(wn->waiter->thr);
                }
            }
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
    for (unsigned i = 0; i < waiter->i; i++) {
        poll_event_t *ev = waiter->events[i];
        spinlock_acquire(&ev->lock);

        // find us
        poll_waiter_node_t *wn = ev->h;
        while (wn) {
            if (wn->waiter == waiter) {
                // Pull us out
                if (wn->next) wn->next->prev = wn->prev;
                if (wn->prev) wn->prev->next = wn->next;
                if (wn == ev->h) ev->h = wn->next;
                break;
            }

            wn = wn->next;
        }

        spinlock_release(&ev->lock);

        if (wn != NULL) {
            slab_free(waiter_node_cache, wn);
            RELEASE_WAITER(waiter);
        }
    }

    waiter->i = 0;
}

/**
 * @brief Cleanup a waiter structure
 * Remember to call @c poll_exit
 */
void poll_destroyWaiter(poll_waiter_t *waiter) {
    RELEASE_WAITER(waiter);
}

/**
 * @brief Free waiter
 */
static void poll_freeWaiter(poll_waiter_t *w) {
    kfree(w->events);
    slab_free(waiter_cache, w);
}

int poll_initCaches() {
    waiter_cache = slab_createCache("poll waiter cache", SLAB_CACHE_DEFAULT, sizeof(poll_waiter_t), 0, NULL, NULL);
    waiter_node_cache = slab_createCache("poll waiter node cache", SLAB_CACHE_DEFAULT, sizeof(poll_waiter_node_t), 0, NULL, NULL);
    return 0;
}

KERN_EARLY_INIT_ROUTINE(poll, INIT_FLAG_DEFAULT, poll_initCaches);
