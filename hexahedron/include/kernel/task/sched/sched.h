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
#include <stdint.h>

/**** TYPES ****/

struct thread;

typedef struct sched {
    char *name;

    struct {
        // initialize scheduler
        void (*sched_init)();

        // initialize scheduler (AP-specific)
        void (*sched_ap)();

        // insert thread (not used when rescheduling)
        void (*sched_insert)(struct thread *);

        // get a new thread
        struct thread* (*sched_get)(void);

        // yield a thread back (called after sched_get() in all cases)
        void (*sched_yield)(struct thread *);
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

#endif
