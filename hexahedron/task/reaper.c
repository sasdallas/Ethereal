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
#include <structs/queue_rb.h>

event_t reap_event = { 0 };
spinlock_t reap_lock = SPINLOCK_INITIALIZER;
queue_rb_t reaper_queue = { 0 };

/**
 * @brief grim reaper
 */
void reaper_proc(void *context) {
    while (1) {
        event_listener_t l;
        EVENT_INIT_LISTENER(&l);
        EVENT_ATTACH(&l, &reap_event);

        spinlock_acquireRaw(&reap_lock);
        while (!queue_rb_empty(&reaper_queue)) {
            process_t *p;
            if (queue_rb_pop(&reaper_queue, (void**)&p)) {
                break;
            }

            spinlock_releaseRaw(&reap_lock);
            process_destroy(p);
            spinlock_acquireRaw(&reap_lock);
        }

        spinlock_releaseRaw(&reap_lock);
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
    spinlock_acquireRaw(&reap_lock);
    queue_rb_push(&reaper_queue, proc);
    spinlock_releaseRaw(&reap_lock);
    EVENT_SIGNAL(&reap_event);
}

/**
 * @brief Reaper initialize
 */
int reaper_init() {
    EVENT_INIT(&reap_event);
    QUEUE_RB_INIT(&reaper_queue, 256);
    process_t *reaper = process_createKernel("reaper", PROCESS_KERNEL, reaper_proc, NULL);
    sched_insert(reaper->main_thread);

    return 0;
}

SCHED_INIT_ROUTINE(reaper, INIT_FLAG_DEFAULT, reaper_init);
