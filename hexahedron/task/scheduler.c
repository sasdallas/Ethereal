/**
 * @file hexahedron/task/scheduler.c
 * @brief Task scheduler for Hexahedron
 * 
 * Implements a priority-based round robin scheduling algorithm
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <kernel/task/process.h>
#include <kernel/task/scheduler.h>
#include <kernel/misc/spinlock.h>
#include <kernel/drivers/clock.h>
#include <kernel/mm/alloc.h>
#include <kernel/fs/kernelfs.h>
#include <kernel/debug.h>
#include <kernel/panic.h>
#include <assert.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "TASK:SCHED", __VA_ARGS__)

/* Next CPU */
int scheduler_next_cpu_to_load_balance = 0;
bool scheduler_load_balancing_enabled = false;

/**
 * @brief Scheduler tick method, called every update
 * @returns 1 to preempt
 */
int scheduler_update(uint64_t ticks) {
    return 1;
}

/**
 * @brief Initialize the scheduler
 */
void scheduler_init() {
    if (processor_count > 1) {
        scheduler_load_balancing_enabled = true;
    }

    scheduler_initCPU();
}

/**
 * @brief Initilaize the scheduler for a CPU
 */
void scheduler_initCPU() {
    current_cpu->sched.queue = list_create("scheduler deque");
    current_cpu->sched.lock = spinlock_create("scheduler lock");
    current_cpu->sched.state = SCHEDULER_STATE_ACTIVE;
}


/**
 * @brief Queue in a new thread
 * @param thread The thread to queue in
 * @returns 0 on success
 */
int scheduler_insertThread(thread_t *thread) {
    assert(current_cpu->sched.state == SCHEDULER_STATE_ACTIVE);
    spinlock_acquire(current_cpu->sched.lock);
    list_append_node(current_cpu->sched.queue, &thread->sched_node);
    spinlock_release(current_cpu->sched.lock);

    return 0;
}

/**
 * @brief Remove a thread from the queue
 * @param thread The tread to remove
 * @returns 0 on success
 */
int scheduler_removeThread(thread_t *thread) {
    return 1;
}

/**
 * @brief Reschedule the current thread
 * 
 * Whenever a thread hits 0 on its timeslice, it is automatically popped and
 * returned to the back of the list.
 */
void scheduler_reschedule() {
}

/**
 * @brief Find the most loaded CPU
 */
int scheduler_findMostLoadedCPU() {
    int most_loaded = -1;
    size_t highest_length = 0;
    for (int i = 0; i < processor_count; i++) {
        if (processor_data[i].sched.state == SCHEDULER_STATE_ACTIVE && processor_data[i].sched.queue->length > highest_length) {
            most_loaded = i;
            highest_length = processor_data[i].sched.queue->length;
        }
    }

    return most_loaded;
}

/**
 * @brief Get the next thread to switch to
 * @returns A pointer to the next thread
 */
thread_t *scheduler_get() {
    if (current_cpu->sched.state != SCHEDULER_STATE_ACTIVE) {
        return current_cpu->idle_process->main_thread;
    }

extern void sleep_callback(uint64_t t);
    sleep_callback(0);

    spinlock_acquire(current_cpu->sched.lock);
    
    node_t *n = list_popleft(current_cpu->sched.queue);
    thread_t *t = current_cpu->idle_process->main_thread;
    if (n) {
        t = n->value;
        /* We have a local thread, release our lock and continue */
        spinlock_release(current_cpu->sched.lock);
    } else {
        /* Nothing on our queue — release our lock before attempting to steal
           to avoid holding two locks at once and potential deadlocks. */
        spinlock_release(current_cpu->sched.lock);

        int cpu_to_steal_from = scheduler_findMostLoadedCPU();
        if (cpu_to_steal_from != -1) {
            spinlock_acquire(processor_data[cpu_to_steal_from].sched.lock);
            n = list_popleft(processor_data[cpu_to_steal_from].sched.queue);
            if (n) {
                t = n->value;
            }
            spinlock_release(processor_data[cpu_to_steal_from].sched.lock);
        }
    }

    /* Ensure we always have a valid thread pointer (fallback to idle). */
    if (!t) {
        t = current_cpu->idle_process->main_thread;
    }

    if (t->status & THREAD_STATUS_STOPPING) {
        LOG(DEBUG, "Killing thread %p since it says its stopping\n", t);
        thread_destroy(t);
        return scheduler_get();
    }

    assert(!(t->status & THREAD_STATUS_STOPPED));
    assert(t);
    return t;
}