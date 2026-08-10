/**
 * @file hexahedron/include/kernel/task/workqueue.h
 * @brief Workqueue implementation
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#ifndef KERNEL_TASK_WORKQUEUE_H
#define KERNEL_TASK_WORKQUEUE_H

/**** INCLUDES ****/
#include <kernel/task/process.h>
#include <structs/list.h>
#include <stdint.h>

/**** DEFINITIONS ****/

#define WORKQUEUE_DEFAULT           0x0

/**** TYPES ****/

struct work;

typedef struct workqueue {
    char *name;
    spinlock_t lock;
    thread_t *work_thread;
    wait_queue_t waiters;
    unsigned int flags;
    STAILQ_ENTRY(struct workqueue) node;
    STAILQ_HEAD(pending_work, struct work);
} workqueue_t;

typedef struct work {
    STAILQ_ENTRY(struct work) node;
    void (*work_callback)(void *context);
    void *work_context;
    bool is_queued;
} work_t;

/**** MACROS ****/

#define WORK_DEFINE(name, callback, context) work_t name = { .work_callback = (callback), .work_context = (context), .is_queued = false }
#define WORK_INIT(var, callback, context) ({ (var)->is_queued = false; (var)->work_callback = (void*)(callback); (var)->work_context = (void*)(context); })

/**** FUNCTIONS ****/

/**
 * @brief Create a new workqueue
 * @param name The name of the workqueue
 * @param flags The flags of the workqueue
 * @returns A new workqueue object
 */
workqueue_t *workqueue_create(char *name, unsigned int flags);

/**
 * @brief Queue work on a workqueue
 * @param workqueue The workqueue to queue on
 * @param work The work to queue
 * @returns 0 on success
 */
int workqueue_add(workqueue_t *workqueue, work_t *work);

/**
 * @brief Destroy a workqueue
 * @param workqueue The workqueue to destroy
 */
void workqueue_destroy(workqueue_t *workqueue);

#endif
