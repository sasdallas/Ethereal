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
#include <kernel/task/sleep.h>
#include <kernel/mem/alloc.h>
#include <kernel/panic.h>
#include <structs/list.h>
#include <kernel/debug.h>
#include <kernel/misc/util.h>
#include <string.h>
#include <assert.h>

/* Time lock */
spinlock_t time_lock = { 0 };

/* Log method */
#define LOG(status, ...) dprintf_module(status, "TASK:SLEEP", __VA_ARGS__)

struct internal_time_queue_entry {
    struct internal_time_queue_entry *next;
    thread_t *sl;
    unsigned long seconds;
    unsigned long subseconds;
};

/* Time queue */
static struct internal_time_queue_entry dummy = { .next = NULL, .sl = NULL };
static struct internal_time_queue_entry *head = &dummy;


/**
 * @brief Wakeup sleepers callback
 * @param ticks Current clock ticks
 */
void sleep_callback(uint64_t ticks) {
    if (!spinlock_tryAcquire(&time_lock)) {
        return;
    }

    // We own the lock
    unsigned long seconds, subseconds;
    clock_getCurrentTime(&seconds, &subseconds);

    struct internal_time_queue_entry *prev = head;
    struct internal_time_queue_entry *n = head->next;

    while (n) {
        if (!n->sl) {
            prev->next = n->next;
            n = prev->next;
            continue;
        }
        if ((n->sl->status & THREAD_STATUS_SLEEPING) == 0) {
            prev->next = n->next;
            n = prev->next;
            continue;
        }

        // Check for expiration
        if (seconds > n->seconds || (seconds == n->seconds && subseconds > n->subseconds)) {
            // Trigger thread wakeup
            sleep_wakeupReason(n->sl, WAKEUP_TIME);
            prev->next = n->next;
            n = prev->next;
            continue;
        }

        prev = n;
        n = n->next;
    }

    spinlock_release(&time_lock);
}

/**
 * @brief Initialize the sleeper system
 */
void sleep_init() {
    
}

/**
 * @brief Put the current thread to sleep
 * 
 * Another thread will wake you up with @c sleep_wakeup
 * Use @c sleep_enter to actually enter the sleep state, which will also return the reason you woke up
 */
void sleep_prepare() {
    spinlock_acquire(&current_cpu->current_thread->sleep.lock);
    current_cpu->current_thread->sleep.seconds = 0;
    current_cpu->current_thread->sleep.subseconds = 0;
    __sync_or_and_fetch(&current_cpu->current_thread->status, THREAD_STATUS_SLEEPING);
}

/**
 * @brief Put the currently thread to sleep until a certain delay
 * 
 * You can still be woken up with @c sleep_wakeup
 * Use @c sleep_enter to actually enter the sleep state, which will also return the reason you woke up
 * 
 * @note If you don't listen to the instructions for this function you will fuck the whole kernel
 * 
 * @param seconds The seconds to sleep
 * @param subseconds Subseconds to sleep for
 */
void sleep_time(unsigned long seconds, unsigned long subseconds) {
    sleep_prepare();
    
    // !!!: As a bit of backstory, I'm about halfway done with the VM rewrite. I have already rewritten the entire sleep system. I do NOT care enough to make this look good right now.
    current_cpu->current_thread->sleep.seconds = seconds;
    current_cpu->current_thread->sleep.subseconds = subseconds;
}

/**
 * @brief Wakeup another thread for a reason
 * @param thread The thread to wakeup
 * @param reason The reason to wake the thread up
 */
int sleep_wakeupReason(struct thread *thread, int reason) {
    if (reason != WAKEUP_TIME && (thread->sleep.seconds || thread->sleep.subseconds)) {
        spinlock_acquire(&time_lock);
        struct internal_time_queue_entry *prev = head;
        struct internal_time_queue_entry *n = head->next;
        while (n) {
            if (n->sl == thread) {
                prev->next = n->next;
                n = prev->next;
                continue;
            }
            prev = n;
            n = n->next;
        }
        spinlock_release(&time_lock);
    }

    spinlock_acquire(&thread->sleep.lock);

    if ((thread->status & THREAD_STATUS_SLEEPING) == 0) {
        spinlock_release(&thread->sleep.lock);
        return 1;
    }

    // Thread is no longer sleeping
    __sync_and_and_fetch(&thread->status, ~(THREAD_STATUS_SLEEPING));
    __atomic_store_n(&thread->sleep.wakeup_reason, reason, __ATOMIC_SEQ_CST);

    spinlock_release(&thread->sleep.lock);
    scheduler_insertThread(thread);
    return 0;
}

/**
 * @brief Immediately trigger an early wakeup on a thread
 * @param thread The thread to wake up
 * @returns 0 on success
 */
int sleep_wakeup(struct thread *thread) {
    return sleep_wakeupReason(thread, WAKEUP_ANOTHER_THREAD);
}

/**
 * @brief Enter sleeping state now
 * @returns A sleep wakeup reason
 */
int sleep_enter() {
    if (current_cpu->current_thread->sleep.seconds || current_cpu->current_thread->sleep.subseconds) {
        // We know the drill...
        // !!!: A full time rewrite is necessitated
        unsigned long seconds, subseconds;
        clock_getCurrentTime(&seconds, &subseconds);

        spinlock_acquire(&time_lock);
        struct internal_time_queue_entry ent = {
            .next = NULL,
            .sl = current_cpu->current_thread,
            .seconds = seconds + current_cpu->current_thread->sleep.seconds,
            .subseconds = subseconds + current_cpu->current_thread->sleep.subseconds,
        };
        
        struct internal_time_queue_entry *n = head;
        while (n->next) n = n->next;
        n->next = &ent;
        spinlock_release(&time_lock);
    }

    
    process_yield(0);
    
    // Clear seconds and subseconds
    current_cpu->current_thread->sleep.seconds = current_cpu->current_thread->sleep.subseconds = 0;

    return __atomic_load_n(&current_cpu->current_thread->sleep.wakeup_reason, __ATOMIC_SEQ_CST);
}

/**
 * @brief Create a new sleep queue
 * @param name Optional name of the sleep queue
 * @returns Sleep queue object
 */
sleep_queue_t *sleep_createQueue(char *name) {
    sleep_queue_t *queue = kzalloc(sizeof(sleep_queue_t));
     
    // Everything else should be initialized
    return queue;
}

/**
 * @brief Put yourself in a sleep queue without preparing
 * @param queue The queue to sleep in
 */
int sleep_addQueue(sleep_queue_t *queue) {
    spinlock_acquire(&queue->lock);

    current_cpu->current_thread->sleep.next = NULL;
    current_cpu->current_thread->sleep.thread = current_cpu->current_thread;

    // Place ourselves in the queue
    if (queue->head) {
        thread_sleep_t *s = queue->head;
        while (s->next) s = s->next;
        s->next = &(current_cpu->current_thread->sleep);
    } else {
        queue->head = &current_cpu->current_thread->sleep;
    }

    spinlock_release(&queue->lock);
    return 0;
}

/**
 * @brief Put yourself in a sleep queue
 * @param queue The queue to sleep in
 * @returns 0 on success. Use sleep_enter to enter your slee
 */
int sleep_inQueue(sleep_queue_t *queue) {
    spinlock_acquire(&queue->lock);

    current_cpu->current_thread->sleep.next = NULL;
    current_cpu->current_thread->sleep.thread = current_cpu->current_thread;

    // Place ourselves in the queue
    if (queue->head) {
        thread_sleep_t *s = queue->head;
        while (s->next) s = s->next;
        s->next = &(current_cpu->current_thread->sleep);
    } else {
        queue->head = &current_cpu->current_thread->sleep;
    }

    // Prepare (acquires the thread's sleep lock while still holding queue->lock to keep ordering)
    sleep_prepare();

    spinlock_release(&queue->lock);
    return 0;
}

/**
 * @brief Wakeup threads in a sleep queue
 * @param queue The queue to start waking up
 * @param amount The amount of threads to wakeup. 0 wakes them all up
 * @returns Amount of threads awoken
 */
int sleep_wakeupQueue(sleep_queue_t *queue, int amounts) {
    int amnt = 0;
 
    spinlock_acquire(&queue->lock);
    thread_sleep_t *t = queue->head;
    thread_sleep_t *prev = NULL;
    while (t) {
        if (amounts != 0 && amnt >= amounts) break;
        
        thread_sleep_t *next = t->next;

        // Unlink t from the queue
        if (prev) {
            prev->next = next;
        } else {
            // removing head
            queue->head = next;
        }

        // Wakeup the thread (outside of list structure)
        sleep_wakeup(t->thread);

        amnt++;
        // prev remains the same because we've removed t from the list
        t = next;
    }
    spinlock_release(&queue->lock);

    return amnt;
}

/**
 * @brief Change your mind and unprepare this thread for sleep
 * @param thread The thread to unprepare from sleep
 * @returns 0 on success
 * @warning Usage of this is not recommended.
 */
int sleep_exit(thread_t *thr) {
    // !!!: Most likely very dangerous...
    __sync_and_and_fetch(&thr->status, ~(THREAD_STATUS_SLEEPING));
    spinlock_release(&thr->sleep.lock);
    return 0;
}

/**
 * @brief Check if you are currently ready to sleep
 */
int sleep_isSleeping() {
    STUB(); // shouldn't be using this anyway
}