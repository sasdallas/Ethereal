/**
 * @file hexahedron/task/workqueue.c
 * @brief Workqueue implementation
 * 
 * @todo global thread pool instead of one per workqueue
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#include <kernel/task/workqueue.h>
#include <kernel/mm/slab.h>
#include <kernel/debug.h>
#include <kernel/init.h>
#include <string.h>

/* Workqueue data */
slab_cache_t *workqueue_cache = NULL;

/* List of all workqueues */
mutex_t workqueue_list_lock = MUTEX_INITIALIZER;
STAILQ_HEAD(workqueues, workqueue_t);

/* Kernel worker process */
process_t *worker_process = NULL;

/* Log method */
#define LOG(status, ...) dprintf_module(status, "TASK:WORKQUEUE", __VA_ARGS__)

/**
 * @brief Drain loop
 */
void workqueue_drain(void *context) {
    workqueue_t *wq = context;
    
    while (1) {
        WAIT_QUEUE_CONDITION(&wq->waiters, STAILQ_FIRST(&wq->pending_work) != NULL);

        // Pop from the workqueue
        void (*cb)(void*) = NULL;
        void *context = NULL;

        spinlock_acquire(&wq->lock);
        
        work_t *work = STAILQ_FIRST(&wq->pending_work);
        if (work) {
            STAILQ_REMOVE_HEAD(&wq->pending_work, node);
            
            // Get the work details then allow it to be queued back
            cb = work->work_callback;
            context = work->work_context;
            work->is_queued = false;
        }
        
        spinlock_release(&wq->lock);

        if (work == NULL) continue;

        cb(context);
    }
}

/**
 * @brief Create a new workqueue
 * @param name The name of the workqueue
 * @param flags The flags of the workqueue
 * @returns A new workqueue object
 */
workqueue_t *workqueue_create(char *name, unsigned int flags) {
    workqueue_t *wq = slab_allocate(workqueue_cache);
    SPINLOCK_INIT(&wq->lock);
    STAILQ_INIT(&wq->pending_work);
    WAIT_QUEUE_INIT(&wq->waiters);
    wq->name = name;
    wq->flags = flags;

    assert(worker_process != NULL);
    wq->work_thread = process_createKernelThread(worker_process, PROCESS_KERNEL, workqueue_drain, wq);
    
    mutex_acquire(&workqueue_list_lock);
    STAILQ_INSERT_TAIL(&workqueues, wq, node);
    mutex_release(&workqueue_list_lock);

    sched_insert(wq->work_thread);
    return wq;
}

/**
 * @brief Queue work on a workqueue
 * @param workqueue The workqueue to queue on
 * @param work The work to queue
 * @returns 0 on success
 */
int workqueue_add(workqueue_t *workqueue, work_t *work) {
    spinlock_acquire(&workqueue->lock);

    if (work->is_queued) {
        spinlock_release(&workqueue->lock);
        return 1;
    }

    STAILQ_INSERT_TAIL(&workqueue->pending_work, work, node);
    work->is_queued = true;

    spinlock_release(&workqueue->lock);
    
    waitqueue_wakeup(&workqueue->waiters, 1);
    return 0;
}

/**
 * @brief Destroy a workqueue
 * @param workqueue The workqueue to destroy
 */
void workqueue_destroy(workqueue_t *workqueue) {
    // this is actually a nightmare
    LOG(ERR, "workqueue_destroy is todo\n");
}

/**
 * @brief Initialize workqueue
 */
int workqueue_init() {
    // TODO: add ability to start kernel process without a main thread
    workqueue_cache = slab_createCache("workqueue", SLAB_CACHE_DEFAULT, sizeof(workqueue_t), 0, NULL, NULL);
    worker_process = process_createKernel("kernel worker", PROCESS_KERNEL, NULL, NULL);
    STAILQ_INIT(&workqueues);
    return 0;
}

SCHED_INIT_ROUTINE(workqueue, INIT_FLAG_DEFAULT, workqueue_init);
