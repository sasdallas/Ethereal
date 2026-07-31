/**
 * @file hexahedron/include/kernel/task/sched/sched.h
 * @brief Kernel scheduler
 * 
 * Hexahedron's kernel API supports selecting different schedulers
 * for the kernel.
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#ifndef KERNEL_TASK_SCHED_SCHED_H
#define KERNEL_TASK_SCHED_SCHED_H

/**** INCLUDES ****/
#include <kernel/misc/util.h>
#include <stdint.h>

/**** TYPES ****/

struct thread;

typedef enum sched_event {
    SCHED_EVENT_DISPATCH,       // thread is dispatching right now
    SCHED_EVENT_DESCHEDULE,     // thread is descheduling right now 
    SCHED_EVENT_SLEEP_ENTER,    // thread just got into sleep
    SCHED_EVENT_SLEEP_WAKEUP,   // thread just woke up from sleep
    SCHED_EVENT_EXIT,           // the thread exited
} sched_event_t;

typedef struct sched {
    char *name;

    struct {
        // initialize scheduler
        void (*sched_init)();

        // initialize scheduler (AP-specific)
        void (*sched_ap)();

        // start scheduler (called on all processors)
        // this is done after idle process has initialized
        void (*sched_start)();

        // initialize thread
        void (*sched_thread)(struct thread *);

        // free thread resources
        void (*sched_free)(struct thread *);

        // insert thread (not used when rescheduling)
        void (*sched_insert)(struct thread *);

        // get a new thread
        struct thread* (*sched_get)(void);

        // yield a thread back (called after sched_get() in all cases)
        void (*sched_yield)(struct thread *);

        // scheduler event
        void (*sched_event)(struct thread *, sched_event_t);
    } ops;
} sched_t;

/**** VARIABLES ****/

/* Current scheduler */
extern sched_t *sched_current;

/**** FUNCTIONS ****/

/**
 * @brief Initialize the scheduler for the BSP
 */
void sched_init();

/**
 * @brief Initialize the scheduler for a sub-processor
 */
void sched_initAP();

/**
 * @brief Start scheduler
 * Only call after idle process has been initialized
 */
static inline void sched_start() {
    return sched_current->ops.sched_start();
}

/**
 * @brief Initialize a thread in the scheduler
 * @param thread The thread to initialize
 */
static inline void sched_initThread(struct thread *thread) {
    if (UNLIKELY(sched_current == NULL)) {
        // this *can* be the idle thread, so we do unfortunately have to do this check.
        return;
    }
    
    return sched_current->ops.sched_thread(thread);
}

/**
 * @brief Free thread scheduler resources
 * @param thread The thread to initialize
 */
static inline void sched_freeThread(struct thread *thread) {
    return sched_current->ops.sched_free(thread);
}

/**
 * @brief Insert a thread into the scheduler
 * @param thread The thread to insert
 */
static inline void sched_insert(struct thread *thread) {
    return sched_current->ops.sched_insert(thread);
}

/**
 * @brief Pop a thread from the scheduler
 */
static inline struct thread *sched_get() {
    return sched_current->ops.sched_get();
}

/**
 * @brief Yield a thread back to the scheduler
 * @param thread The thread to yield back
 */
static inline void sched_yield(struct thread *thread) {
    return sched_current->ops.sched_yield(thread);
}

/**
 * @brief Process scheduler event on thread
 * @param thread The thread to process the event on
 * @param event The event
 */
static inline void sched_event(struct thread *thread, sched_event_t event) {
    return sched_current->ops.sched_event(thread, event);
}

#endif
