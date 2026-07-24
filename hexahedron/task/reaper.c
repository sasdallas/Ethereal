/**
 * @file hexahedron/task/reaper.c
 * @brief evil grim reaper
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#include <kernel/task/process.h>
#include <kernel/init.h>
#include <structs/queue.h>

event_t reap_event = { 0 };
mutex_t reap_lock = MUTEX_INITIALIZER;
queue_t reaper_queue = { 0 };

/**
 * @brief grim reaper
 */
void reaper_proc(void *context) {
    while (1) {
        event_listener_t l;
        EVENT_INIT_LISTENER(&l);
        EVENT_ATTACH(&l, &reap_event);

        mutex_acquire(&reap_lock);
        while (!queue_empty(&reaper_queue)) {
            process_t *p = queue_pop(&reaper_queue);
            mutex_release(&reap_lock);
            process_destroy(p);
            mutex_acquire(&reap_lock);
        }

        mutex_release(&reap_lock);
        EVENT_WAIT(&l, -1);
        EVENT_DETACH(&l);
        EVENT_DESTROY_LISTENER(&l);
    }
}

/**
 * @brief Push a process onto the reaper
 * @param proc The process to push
 */
void reaper_push(process_t *proc) {
    mutex_acquire(&reap_lock);
    queue_push(&reaper_queue, proc);
    mutex_release(&reap_lock);
    EVENT_SIGNAL(&reap_event);
}

/**
 * @brief Reaper initialize
 */
int reaper_init() {
    EVENT_INIT(&reap_event);
    process_t *reaper = process_createKernel("reaper", PROCESS_KERNEL, PRIORITY_HIGH, reaper_proc, NULL);
    scheduler_insertThread(reaper->main_thread);

    return 0;
}

SCHED_INIT_ROUTINE(reaper, INIT_FLAG_DEFAULT, reaper_init);
