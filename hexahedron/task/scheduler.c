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
#include <kernel/mem/alloc.h>
#include <kernel/debug.h>
#include <kernel/panic.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "TASK:SCHED", __VA_ARGS__)

/* Scheduler timeslices */
time_t scheduler_timeslices[] = {
    [PRIORITY_HIGH] = 5,
    [PRIORITY_MED] = 4,
    [PRIORITY_LOW] = 3,
};

/* Thread queue */
list_t *thread_queue = NULL;

/* Scheduler lock */
spinlock_t scheduler_lock = { 0 };


/**
 * @brief Scheduler tick method, called every update
 */
int scheduler_update(uint64_t ticks) {
    // Update the current process' time
    if (!current_cpu->current_thread) {
        return 0; // Before a process was initialized
    }

    current_cpu->current_thread->total_ticks = clock_getTickCount();

    current_cpu->current_thread->preempt_ticks--;
    if (current_cpu->current_thread->preempt_ticks <= 0) {
        // Get out of here, you're out of your timeslice
        scheduler_reschedule();
        return 1;
    }

    return 0;
}

/**
 * @brief Initialize the scheduler
 */
void scheduler_init() {
    thread_queue = list_create("thread queue");
    LOG(INFO, "Scheduler initialized\n");
}


/**
 * @brief Queue in a new thread
 * @param thread The thread to queue in
 * @returns 0 on success
 */
int scheduler_insertThread(thread_t *thread) {
    if (!thread || !thread_queue) return -1;

    spinlock_acquire(&scheduler_lock);
    if (thread->sched_node) {
        list_append_node(thread_queue, thread->sched_node);
    } else {
        list_append(thread_queue, (void*)thread);
        thread->sched_node = list_find(thread_queue, (void*)thread);
    }
    spinlock_release(&scheduler_lock);

    // LOG(INFO, "Inserted thread %p for process '%s' (priority: %d)\n", thread, thread->parent->name, thread->parent->priority);
    return 0;
}

/**
 * @brief Remove a thread from the queue
 * @param thread The tread to remove
 * @returns 0 on success
 */
int scheduler_removeThread(thread_t *thread) {
    if (!thread) return -1;

    spinlock_acquire(&scheduler_lock);
    node_t *thread_node = thread->sched_node;

    if (!thread_node) {
        LOG(WARN, "Could not delete thread %p (process '%s') because it was not found in the queue\n", thread, thread->parent->name);
        spinlock_release(&scheduler_lock);
        return -1;
    }

    list_delete(thread_queue, thread_node);
    kfree(thread_node); // Node structure is useless
    spinlock_release(&scheduler_lock);

    LOG(INFO, "Removed thread %p for process '%s' (priority: %d)\n", thread, thread->parent->name, thread->parent->priority);
    return 0;
}

/**
 * @brief Reschedule the current thread
 * 
 * Whenever a thread hits 0 on its timeslice, it is automatically popped and
 * returned to the back of the list.
 */
void scheduler_reschedule() {
    if (!current_cpu->current_thread) return;

    // If the current thread is still running, we can append it to the back of the queue
    if (current_cpu->current_thread->status & THREAD_STATUS_RUNNING) {
        spinlock_acquire(&scheduler_lock);
        // Get the thread's timeslice
        current_cpu->current_thread->preempt_ticks = scheduler_timeslices[current_cpu->current_thread->parent->priority];

        spinlock_release(&scheduler_lock);
    }
}

/**
 * @brief Get the next thread to switch to
 * @returns A pointer to the next thread
 */
thread_t *scheduler_get() {
    // Is this core fit to schedule?
    if (current_cpu->idle_process == NULL || current_cpu->idle_process->main_thread == NULL) {
        // Huh
        kernel_panic_extended(UNSUPPORTED_FUNCTION_ERROR, "scheduler", "*** Tried to switch tasks with no queue and no idle task\n");
    }

    // Get lock
    spinlock_acquire(&scheduler_lock);

    // Is there a queue and does it have a head?
    if (!thread_queue || !thread_queue->head) {
        // Release lock
        spinlock_release(&scheduler_lock);


        // No thread queue. This likely means a core entered the switcher before scheduling initialized, which is fine.
        // We just need to return the kernel idle task's thread
        return current_cpu->idle_process->main_thread;
    }
    
    node_t *thread_node;
    thread_t *thread;

    do {
        // Pop the next thread off the list
        thread_node = list_popleft(thread_queue);
    
        // Did that work?
        if (!thread_node || !thread_node->value) {
            // Normally this would be fatal, but it just in fact means we've cleared our queue of all running processes
            // This is supposed to be fatal (no init process), but other parts of the code will catch this.
            // Else, guess it just freezes. Who knows!
            return current_cpu->idle_process->main_thread;
        }

        // Get thread and free node
        thread = (thread_t*)(thread_node->value);

        if (thread->status & THREAD_STATUS_STOPPING) {
            // Update status to be STOPPED
            LOG(INFO, "Thread %p was caught in the scheduler and has been shutdown\n");
            __sync_or_and_fetch(&thread->status, THREAD_STATUS_STOPPED);
            thread_destroy(thread);
            continue;
        }

        if (thread->status & THREAD_STATUS_STOPPED) {
            kernel_panic_extended(SCHEDULER_ERROR, "scheduler", "*** Thread %p is corrupt and should not have been owned by the scheduler.\n", thread);
        }

        break;
    } while (1);
    

    // Unlock
    spinlock_release(&scheduler_lock);

    // Return it
    return thread;
}