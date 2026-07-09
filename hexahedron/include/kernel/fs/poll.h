/**
 * @file hexahedron/include/kernel/fs/poll.h
 * @brief Kernel poll mechanism
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef KERNEL_FS_POLL_H
#define KERNEL_FS_POLL_H

/**** INCLUDES ****/
#include <stdint.h>
#include <sys/poll.h>
#include <kernel/misc/spinlock.h>
#include <kernel/refcount.h>
#include <stddef.h>
#include <stdbool.h>

/**** DEFINITIONS ****/

/**** TYPES ****/

typedef uint16_t poll_events_t;
struct poll_event;

typedef struct poll_waiter {
    unsigned char lock;             // General atomic lock, stays locked until poll_wait
    struct thread *thr;             // Thread waiting
    
    size_t nevents;                 // Number of events
    size_t i;                       // Index
    struct poll_event **events;     // Allocated at poll_createWaiter time
    
    bool ready;                     // Early wakey
    refcount_t refs;                // References remaining to this waiter
} poll_waiter_t;

// I literally do not care if this is good or not
typedef struct poll_waiter_node {
    struct poll_waiter_node *next;
    struct poll_waiter_node *prev;
    poll_events_t events;
    poll_waiter_t *waiter;
} poll_waiter_node_t;

typedef poll_events_t (*poll_events_checker_t)(struct poll_event *ev);
typedef struct poll_event {
    spinlock_t lock;
    poll_waiter_node_t *h;
    poll_events_checker_t checker;
    void *dev;
} poll_event_t;

/**** MACROS ****/

#define POLL_EVENT_INIT(e) ({ (e)->h = NULL; SPINLOCK_INIT(&(e)->lock); (e)->checker = NULL; (e)->dev = NULL; })

/**** FUNCTIONS ****/

/**
 * @brief Create and initialize a waiter
 * @param thr The thread to use
 * @param nevents Number of events waiting on
 */
poll_waiter_t *poll_createWaiter(struct thread *thr, size_t nevents);

/**
 * @brief Add an interest on an event
 * @param waiter The waiter to add the interest for
 * @param event The event to wait on
 * @param events Target events
 */
int poll_add(poll_waiter_t *waiter, poll_event_t *event, poll_events_t events);

/**
 * @brief Enter waiting
 * @param waiter The waiter to wait on
 * @param timeout The timeout (or -1 for infinite waiting)
 * @returns Error code (EINTR or ETIMEDOUT)
 */
int poll_wait(poll_waiter_t *waiter, int timeout);

/**
 * @brief Signal any events
 * @param event The event to signal
 * @param events The events to signal
 */
void poll_signal(poll_event_t *event, poll_events_t events);

/**
 * @brief Exit a poll
 * @param waiter The waiter
 */
void poll_exit(poll_waiter_t *waiter);

/**
 * @brief Cleanup a waiter structure
 * Remember to call @c poll_exit
 */
void poll_destroyWaiter(poll_waiter_t *waiter);



#endif