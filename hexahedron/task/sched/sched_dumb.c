/**
 * @file hexahedron/task/sched/sched_dumb.c
 * @brief Dumb scheduler
 * 
 * Extremely dumb scheduler that was part of the Hexahedron pre-2.2
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#include <kernel/task/sched.h>
#include <kernel/task/sched/sched_dumb.h>
#include <kernel/task/process.h>
#include <kernel/processor_data.h>
#include <assert.h>

#define SCHED_THIS() ((sched_dumb_queue_t*)(current_cpu->sched_data))

/* Dumb scheduler */
static void sched_dumb_init();
static void sched_dumb_ap();
static void sched_dumb_start();
static void sched_dumb_insert(thread_t *thread);
static void sched_dumb_tick(void *context);
static thread_t *sched_dumb_get();
static void sched_dumb_thread(thread_t *thread);
static void sched_dumb_free(thread_t *thread);
static void sched_dumb_event(thread_t *thread, sched_event_t event);

sched_t dumb_scheduler = {
    .name = "dumb",
    .ops = {
        .sched_init = sched_dumb_init,
        .sched_ap = sched_dumb_ap,
        .sched_start = sched_dumb_start,
        .sched_thread = sched_dumb_thread,
        .sched_free = sched_dumb_free,
        .sched_insert = sched_dumb_insert,
        .sched_get = sched_dumb_get,
        .sched_yield = sched_dumb_insert, // dumb sched has no difference between the two
        .sched_event = sched_dumb_event,
    }
};


/**
 * @brief Initialize dumb scheduler
 */
static void sched_dumb_init() {
    sched_dumb_ap();
}

/**
 * @brief Initialize dumb scheduler (AP)
 */
static void sched_dumb_ap() {
    sched_dumb_queue_t *queue = kmalloc(sizeof(sched_dumb_queue_t));
    LIST_INIT(&queue->queue);
    SPINLOCK_INIT(&queue->lock);
    timer_init(&queue->timer, sched_dumb_tick, NULL, 10000000, true, "dumb_tick");
    current_cpu->sched_data = queue;

}

/**
 * @brief Start dumb scheduler
 */
static void sched_dumb_start() {
    sched_dumb_queue_t *queue = SCHED_THIS();
    timer_insert(&queue->timer);
}

/**
 * @brief Initialize thread
 */
static void sched_dumb_thread(thread_t *thread) {
    thread->sched = kmalloc(sizeof(node_t));
    node_t *n = thread->sched;
    NODE_INIT(n, thread);
}

/**
 * @brief Free thread data
 */
static void sched_dumb_free(thread_t *thread) {
    kfree(thread->sched);
}

/**
 * @brief Handle scheduler event
 */
static void sched_dumb_event(thread_t *thread, sched_event_t event) {
    // stub
}

/**
 * @brief Insert dumb scheduler
 */
static void sched_dumb_insert(thread_t *thread) {
    if (thread->flags & THREAD_FLAG_IDLE) return;

    sched_dumb_queue_t *q = SCHED_THIS();
    node_t *sched_node = thread->sched;

    spinlock_acquire(&q->lock);
    list_append_node(&q->queue, sched_node);
    spinlock_release(&q->lock);
}

/**
 * @brief Dumb tick
 */
static void sched_dumb_tick(void *context) {
    sched_dumb_queue_t *q = SCHED_THIS();
    if (current_cpu->current_thread && (current_cpu->current_thread->flags & THREAD_STATUS_STOPPED) == 0) {
        if (current_cpu->current_process == current_cpu->idle_process || q->queue.length>0) {
            current_cpu->current_thread->flags |= THREAD_FLAG_NEEDS_RESCHED;
        }
    }
}

/**
 * @brief Find most loaded CPU using stupid algorithm
 * Most threads != most loaded. This is also extremely racey.
 */
static sched_dumb_queue_t *sched_dumb_find() {
    sched_dumb_queue_t *most_loaded = NULL;
    size_t highest_len = 0;

    for (int i = 0; i < processor_count; i++) {
        sched_dumb_queue_t *this = (sched_dumb_queue_t*)processor_data[i].sched_data;
        if (this) {
            // !!! race!
            if (this->queue.length > highest_len) {
                highest_len = this->queue.length;
                most_loaded = this;
            }
        }
    }

    return most_loaded;
}


/**
 * @brief Get dumb scheduler
 */
static thread_t *sched_dumb_get() {
    sched_dumb_queue_t *q = SCHED_THIS();

    if (q == NULL) {
        return current_cpu->idle_process->main_thread;
    }

    // !!! awful but will be fixed i promise
extern void sleep_callback();
    sleep_callback();

    // Get a thread from the queue
    spinlock_acquire(&q->lock);
    node_t *n = list_popleft(&q->queue);
    spinlock_release(&q->lock);

    thread_t *thr;
    if (n) {
        thr = n->value;
    } else {
        thr = current_cpu->idle_process->main_thread;

        // maybe we can steal
        if (processor_count>1) {
            sched_dumb_queue_t *steal = sched_dumb_find();
            if (steal) {
                spinlock_acquire(&steal->lock);
                n = list_popleft(&steal->queue);
                spinlock_release(&steal->lock);

                if (n) {
                    thr = n->value;
                }
            }
        }
    }

    return thr;
}
