/**
 * @file hexahedron/task/timer.c
 * @brief Timer system
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/task/process.h>
#include <kernel/drivers/clock.h>
#include <kernel/mm/alloc.h>
#include <kernel/misc/spinlock.h>
#include <kernel/debug.h>
#include <structs/list.h>

/* Global timer queue */
list_t *timer_queue = NULL;

/* Timer process */
process_t *timer_process = NULL;

/* Timer lock */
spinlock_t timer_lock = { 0 };

/* Log method */
#define LOG(status, ...) dprintf_module(status, "TASK:TIMER", __VA_ARGS__)

/**
 * @brief Timer callback from process
 */
void timer_kthread(void *ctx) {
    for (;;) {
        if (!timer_queue->length) {
            LOG(DEBUG, "No timers are available\n");
            sleep_prepare();
            sleep_enter();
        }

        unsigned long seconds, subseconds;
        clock_getCurrentTime(&seconds, &subseconds);

        long sleep_seconds = -1;
        long sleep_subseconds = -1;

        spinlock_acquire(&timer_lock);

        foreach(node, timer_queue) {
            process_timer_t *t = (process_timer_t*)node->value;

            if (t->which == ITIMER_REAL) {
                // Quick check! Never know what race conditions can occur when processes are exiting.
                if (!t->value.tv_sec && !t->value.tv_usec) {
                    LOG(DEBUG, "Removing timer from queue as it has nothing left\n");
                    list_delete(timer_queue, node);
                    kfree(node);

                    t->expire_seconds = 0;
                    t->expire_subseconds = 0;
                    continue;
                } 

                // Now actually check
                if (t->expire_seconds < seconds || (t->expire_seconds == seconds && t->expire_subseconds < subseconds)) {
                    // Timer has expired
                    LOG(DEBUG, "itimer has expired on process %p, sending SIGALRM and resetting timer\n", t->process);

                    t->value.tv_sec = 0;
                    t->value.tv_usec = 0;

                    if (t->reset_value.tv_sec || t->reset_value.tv_usec) {
                        // There is a reset value
                        t->value.tv_sec = t->reset_value.tv_sec;
                        t->value.tv_usec = t->reset_value.tv_usec;
                        clock_relative(t->reset_value.tv_sec, t->reset_value.tv_usec, &t->expire_seconds, &t->expire_subseconds);
                        
                    }

                    signal_send(t->process, SIGALRM);
                } 
                
                if (t->value.tv_sec || t->value.tv_usec) {
                    // Check expiration time
                    if (sleep_seconds == -1 || (sleep_seconds < t->value.tv_sec || (sleep_seconds == t->value.tv_sec && sleep_subseconds < t->value.tv_usec))) {
                        sleep_seconds = t->value.tv_sec;
                        sleep_subseconds = t->value.tv_usec;
                    }
                }

                // Another check
                if (!t->value.tv_sec && !t->value.tv_usec) {
                    LOG(DEBUG, "Removing timer from queue as it has nothing left\n");
                    list_delete(timer_queue, node);
                    // kfree(node);

                    t->expire_seconds = 0;
                    t->expire_subseconds = 0;
                } 
            } else {
                // TODO: ITIMER_VIRTUAL, ITIMER_PROF
                LOG(ERR, "ITIMER_VIRTUAL/ITIMER_PROF is not supported (got %d).\n", t->which);
                list_delete(timer_queue, node);
            }
        }
        
        spinlock_release(&timer_lock);

        if (sleep_seconds == -1) {
            sleep_prepare(); // Anything in the sleep queue wakes us up.
        } else {
            LOG(DEBUG, "Sleeping %d seconds %d useconds\n", sleep_seconds, sleep_subseconds);
            sleep_time(sleep_seconds, sleep_subseconds); // TODO: Maybe don't hardcode?
        }

        sleep_enter();
    }
}

/**
 * @brief Set and queue a timer for a process
 * @param process The process to queue the timer for
 * @param which The type of timer to queue
 * @param value The new value of the timer
 * @returns 0 on success
 */
int timer_set(struct process *process, int which, struct itimerval *value) {
    if (!timer_queue) {
        // Initialize everything
        timer_queue = list_create("timer queue");
        timer_process = process_createKernel("timer_process", PROCESS_KERNEL, PRIORITY_MED, timer_kthread, NULL);
        scheduler_insertThread(timer_process->main_thread);
    }

    spinlock_acquire(&timer_lock);

    process_timer_t *timer = (process_timer_t*)&process->itimers[which];
    timer->process = process;
    timer->which = which;

    timer->value.tv_sec = value->it_value.tv_sec;
    timer->value.tv_usec = value->it_value.tv_usec;

    timer->reset_value.tv_sec = value->it_interval.tv_sec;
    timer->reset_value.tv_usec = value->it_interval.tv_usec;
    
    if (timer->value.tv_sec || timer->value.tv_usec) {
        clock_relative(value->it_value.tv_sec, value->it_value.tv_usec, &timer->expire_seconds, &timer->expire_subseconds);
        list_append(timer_queue, timer);

        // Wakeup handler
        sleep_wakeup(timer_process->main_thread);
    }

    spinlock_release(&timer_lock);

    LOG(DEBUG, "Created a new timer with value %d/%d\n", timer->value.tv_sec, timer->value.tv_usec);

    return 0; 
}