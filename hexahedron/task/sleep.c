/**
 * @file hexahedron/task/sleep.c
 * @brief Thread blocker/sleeper handler
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <kernel/task/process.h>
#include <kernel/drivers/clock.h>
#include <kernel/task/sleep.h>
#include <kernel/mem/alloc.h>
#include <kernel/panic.h>
#include <structs/list.h>
#include <kernel/debug.h>
#include <string.h>
#include <assert.h>

/* Sleeping queue */
list_t *sleep_queue = NULL;

/* Sleep queue lock */
spinlock_t sleep_queue_lock = { 0 };

/* Log method */
#define LOG(status, ...) dprintf_module(status, "TASK:SLEEP", __VA_ARGS__)

/**
 * @brief Wakeup state to string
 */
char *sleep_wakeupToString(int wakeup) {
    switch (wakeup) {
        case SLEEP_FLAG_NOCOND:
            return "NEVER";
        case SLEEP_FLAG_TIME:
            return "TIME";
        case SLEEP_FLAG_COND:
            return "UPON_CONDITION";
        case SLEEP_FLAG_WAKEUP:
            return "NOW";
        default:
            if (wakeup >= WAKEUP_SIGNAL) {
                return "WOKEN_UP_ALREADY";
            }

            return "???";
    }
}

/**
 * @brief Wakeup sleepers callback
 * @param ticks Current clock ticks
 */
void sleep_callback(uint64_t ticks) {
    if (!sleep_queue) return;

    // Get time for any threads that need it
    unsigned long seconds, subseconds;
    clock_getCurrentTime(&seconds, &subseconds);
    
    if (!spinlock_tryAcquire(&sleep_queue_lock)) {
        return;
    }


    node_t *node = sleep_queue->head;
    while (node) {
        thread_sleep_t *sleep = (thread_sleep_t*)node->value;
        if (!sleep || !sleep->thread) {
            LOG(WARN, "Corrupt node in sleep queue %p (sleep: %p)\n", node, sleep);
            
            kernel_panic_prepare(UNKNOWN_CORRUPTION_DETECTED);
            dprintf(NOHEADER, "*** Detected corruption in kernel sleep queue\n");
            dprintf(NOHEADER, "*** This usually indicates a race condition in the kernel, check all systems using sleep_wakeup and that they lock.\n\n");
            dprintf(NOHEADER, "*** The failing list node: %p\n", node);

            if (!sleep) {
                dprintf(NOHEADER, "*** The failing sleep queue entry is NULL\n");
            } else {
                dprintf(NOHEADER, "*** The failing sleep queue entry: %p\n", sleep);
                dprintf(NOHEADER, "*** Was supposed to wakeup %s but lost its thread structure (context=%p)\n", sleep_wakeupToString(sleep->sleep_state), sleep->context);
            }
            
            kernel_panic_finalize();
        }


        int wakeup = 0;

        if (sleep->sleep_state >= WAKEUP_SIGNAL) {
            wakeup = sleep->sleep_state; // Some other thread already marked this one as time to wakey wakey
        } else if (sleep->thread->pending_signals) {
            // Yes, get out!
            wakeup = WAKEUP_SIGNAL;
        } else if (sleep->sleep_state == SLEEP_FLAG_NOCOND) {
            node = node->next;
            continue; // Thread doesn't need to wakeup
        } else if (sleep->sleep_state == SLEEP_FLAG_TIME) {
            // We're sleeping on time.
            if ((sleep->seconds == seconds && sleep->subseconds <= subseconds) || (sleep->seconds < seconds)) {
                // Wakeup now
                // LOG(DEBUG, "WAKEUP: Time passed, waking up thread %p\n", sleep->thread);
                wakeup = WAKEUP_TIME;
            }
        } else if (sleep->sleep_state == SLEEP_FLAG_COND) {
            if (!sleep->condition) {
                LOG(WARN, "Corrupt node in sleep queue has SLEEP_FLAG_COND but has no condition (sleep state %p, thread %p)\n", sleep, sleep->thread);
                continue;
            }

            if (sleep->condition(sleep->thread, sleep->context)) {
                LOG(DEBUG, "WAKEUP: Condition success, waking up thread %p\n", sleep->thread);
                wakeup = WAKEUP_COND;
            }
        } else if (sleep->sleep_state == SLEEP_FLAG_WAKEUP) {
            // Immediately wakeup
            // LOG(DEBUG, "WAKEUP: Immediately waking up thread %p\n", sleep->thread);
            wakeup = WAKEUP_ANOTHER_THREAD;
        }

        if (wakeup) {
            sleep->sleep_state = wakeup;
            node_t *old = node;
            node = old->next;
            list_delete(sleep_queue, old);
            kfree(old);
            __sync_and_and_fetch(&sleep->thread->status, ~(THREAD_STATUS_SLEEPING));
            scheduler_insertThread(sleep->thread);
        } else {
            // Condition not yet met
            node = node->next;
        }
    }

    spinlock_release(&sleep_queue_lock);
}

/**
 * @brief Initialize the sleeper system
 */
void sleep_init() {
    sleep_queue = list_create("thread sleep queue");
    clock_registerUpdateCallback(sleep_callback);
}


/**
 * @brief Put a thread to sleep, no condition and no way to wakeup without @c sleep_wakeup
 * @param thread The thread to sleep
 * @returns 0 on success
 * 
 * @note If you're putting the current thread to sleep, call @c sleep_enter right after
 */
int sleep_untilNever(struct thread *thread) {
    if (!thread) return 1;

    if (thread->sleep) {
        LOG(ERR, "This thread sleeping already..?\n");
        return 1;
    }

    // Construct a sleep node
    thread_sleep_t *sleep = kmalloc(sizeof(thread_sleep_t));
    memset(sleep, 0, sizeof(thread_sleep_t));
    sleep->sleep_state = SLEEP_FLAG_NOCOND;
    sleep->thread = thread;

#pragma GCC diagnostic ignored "-Wframe-address"
    sleep->context = __builtin_return_address(0);
    thread->sleep = sleep;

    // Manually create node..
    node_t *node = kzalloc(sizeof(node_t));
    node->value = (void*)sleep;
    sleep->node = node;

    // Mark thread as sleeping. If process_yield finds this thread to be trying to reschedule,
    // it will disallow it and just switch away
    __sync_or_and_fetch(&thread->status, THREAD_STATUS_SLEEPING);

    return 0;
}

/**
 * @brief Put a thread to sleep until a specific amount of time in the future has passed
 * @param thread The thread to put to sleep
 * @param seconds Seconds to wait in the future
 * @param subseconds Subseconds to wait in the future
 * @returns 0 on success
 * 
 * @note If you're putting the current thread to sleep, call @c sleep_enter right after
 */
int sleep_untilTime(struct thread *thread, unsigned long seconds, unsigned long subseconds) {
    if (!thread) return 1;

    // Construct a sleep node
    thread_sleep_t *sleep = kmalloc(sizeof(thread_sleep_t));
    memset(sleep, 0, sizeof(thread_sleep_t));
    sleep->sleep_state = SLEEP_FLAG_TIME;
    sleep->thread = thread;
    thread->sleep = sleep;

    // Get relative time
    unsigned long new_seconds, new_subseconds;
    clock_relative(seconds, subseconds, &new_seconds, &new_subseconds);
    sleep->seconds = new_seconds;
    sleep->subseconds = new_subseconds;

    // Setup context
    sleep->context = __builtin_return_address(0);

    // Manually create node..
    node_t *node = kzalloc(sizeof(node_t));
    node->value = (void*)sleep;
    sleep->node = node;

    // Mark thread as sleeping. If process_yield finds this thread to be trying to reschedule,
    // it will disallow it and just switch away
    __sync_or_and_fetch(&thread->status, THREAD_STATUS_SLEEPING);
    return 0;
}

/**
 * @brief Put a thread to sleep until a specific condition is ready
 * @param thread The thread to put to sleep
 * @param condition Condition function
 * @param context Optional context passed to condition function
 * @returns 0 on success
 * 
 * @note If you're putting the current thread to sleep, call @c sleep_enter right after
 */
int sleep_untilCondition(struct thread *thread, sleep_condition_t condition, void *context) {
    if (!thread) return 1;

    // Construct a sleep node
    thread_sleep_t *sleep = kmalloc(sizeof(thread_sleep_t));
    memset(sleep, 0, sizeof(thread_sleep_t));
    sleep->sleep_state = SLEEP_FLAG_COND;
    sleep->thread = thread;
    thread->sleep = sleep;

    // Set condition
    sleep->condition = condition;
    sleep->context = context;

    // Manually create node..
    node_t *node = kzalloc(sizeof(node_t));
    node->value = (void*)sleep;

    sleep->node = node;

    // Mark thread as sleeping. If process_yield finds this thread to be trying to reschedule,
    // it will disallow it and just switch away
    __sync_or_and_fetch(&thread->status, THREAD_STATUS_SLEEPING);
    return 0;
}

/**
 * @brief Immediately trigger an early wakeup on a thread
 * @param thread The thread to wake up
 * @returns 0 on success
 */
int sleep_wakeup(struct thread *thread) {
    spinlock_acquire(&sleep_queue_lock);
    
    if (!thread || !thread->sleep) {
        // This thread isn't sleeping
        spinlock_release(&sleep_queue_lock);
        return 1;
    }
    
    thread_sleep_t *sleep = thread->sleep;
    sleep->sleep_state = SLEEP_FLAG_WAKEUP;
    spinlock_release(&sleep_queue_lock);

    return 0;
}

/**
 * @brief Enter sleeping state now
 * @returns A sleep wakeup reason
 */
int sleep_enter() {
    if (!current_cpu->current_thread->sleep) {
        LOG(WARN, "Thread tried to sleep without a node\n");
        return WAKEUP_ANOTHER_THREAD;
    }

    spinlock_acquire(&sleep_queue_lock);
    list_append_node(sleep_queue, current_cpu->current_thread->sleep->node);
    spinlock_release(&sleep_queue_lock);


    // TODO: Maybe don't yield if thread is already supposed to wakeup? This would mean sleep_callback can't NULL it
    process_yield(0);
    int state = current_cpu->current_thread->sleep->sleep_state;
    kfree(current_cpu->current_thread->sleep);
    current_cpu->current_thread->sleep = NULL;
    return state;
}

/**
 * @brief Create a new sleep queue
 * @param name Optional name of the sleep queue
 * @returns Sleep queue object
 */
sleep_queue_t *sleep_createQueue(char *name) {
    sleep_queue_t *queue = kzalloc(sizeof(sleep_queue_t));
    queue->queue.name = name;
    
    // Everything else should be initialized
    return queue;
}

/**
 * @brief Put yourself in a sleep queue
 * @param queue The queue to sleep in
 * @returns 0 on success. Use sleep_enter to enter your slee
 */
int sleep_inQueue(sleep_queue_t *queue) {
    if (!queue) return 1;
    if (current_cpu->current_thread->sleep) {
        LOG(ERR, "This thread sleeping already..?\n");
        return 1;
    }

    spinlock_acquire(&queue->lock);
    sleep_untilNever(current_cpu->current_thread);
    
    current_cpu->current_thread->sleep->context = __builtin_return_address(0);
    list_append(&queue->queue, (void*)current_cpu->current_thread);
    spinlock_release(&queue->lock);

    return 0; // !!!: Validate that in the time from putting ourselves in this queue to sleeping we can't be woken up, and if so we can be woken up properly.
}

/**
 * @brief Wakeup threads in a sleep queue
 * @param queue The queue to start waking up
 * @param amount The amount of threads to wakeup. 0 wakes them all up
 * @returns Amount of threads awoken
 */
int sleep_wakeupQueue(sleep_queue_t *queue, int amounts) {
    if (!queue) return 0;

    spinlock_acquire(&queue->lock);

    int awoken = 0;

    node_t *node = list_popleft(&queue->queue);
    while (node) {
        thread_t *thr = (thread_t*)node->value;
        if (thr) {
            assert(thr->sleep);
            assert(thr->sleep->sleep_state < WAKEUP_SIGNAL);
            sleep_wakeup(thr);
        }

        // Increase
        awoken++;
        if (awoken >= amounts) { kfree(node); break; }
        
        // Next
        kfree(node);
        node = list_popleft(&queue->queue);
    }

    spinlock_release(&queue->lock);
    return awoken;
}

/**
 * @brief Change your mind and unprepare this thread for sleep
 * @param thread The thread to unprepare from sleep
 * @returns 0 on success
 */
int sleep_exit(thread_t *thr) {
    if (!thr || !thr->sleep) return 1;
    spinlock_acquire(&sleep_queue_lock);
    list_delete(sleep_queue, list_find(sleep_queue, thr->sleep));
    kfree(thr->sleep);
    spinlock_release(&sleep_queue_lock);
    return 0;
}