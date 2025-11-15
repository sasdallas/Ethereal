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

    struct internal_time_queue_entry *n = head;
    struct internal_time_queue_entry *prev = n;

    while (n) {
        if (!n->sl) { n = n->next; continue; }

        // Check for expiration
        if (seconds > n->seconds || (seconds == n->seconds && subseconds > n->subseconds)) {
            // Trigger thread wakeup
            sleep_wakeupReason(n->sl, WAKEUP_TIME);
            prev->next = n->next;
        }

        n = n->next;
    }

    spinlock_release(&time_lock);
}

/**
 * @brief Initialize the sleeper system
 */
void sleep_init() {
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
    LOG(DEBUG, "FUCK OFF (untilNever)\n");
    STUB();
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
 * Use @c sleep_enter to actualyl enter the sleep state, which will also return the reason you woke up
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
 * @brief Put a thread to sleep until a specific amount of time in the future has passed
 * @param thread The thread to put to sleep
 * @param seconds Seconds to wait in the future
 * @param subseconds Subseconds to wait in the future
 * @returns 0 on success
 * 
 * @note If you're putting the current thread to sleep, call @c sleep_enter right after
 */
int sleep_untilTime(struct thread *thread, unsigned long seconds, unsigned long subseconds) {
    STUB();
}

/**
 * @brief Wakeup another thread for a reason
 * @param thread The thread to wakeup
 * @param reason The reason to wake the thread up
 */
int sleep_wakeupReason(struct thread *thread, int reason) {
    spinlock_acquire(&thread->sleep.lock);

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
    if (current_cpu->current_thread->sleep.seconds) {
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
 * @brief Put yourself in a sleep queue
 * @param queue The queue to sleep in
 * @returns 0 on success. Use sleep_enter to enter your slee
 */
int sleep_inQueue(sleep_queue_t *queue) {
    spinlock_acquire(&queue->lock);

    // Place ourselves in the queue
    if (queue->head) {
        thread_sleep_t *s = queue->head;
        while (s->next) s = s->next;
        s->next = &(current_cpu->current_thread->sleep);
    } else {
        queue->head = &current_cpu->current_thread->sleep;
    }

    current_cpu->current_thread->sleep.next = NULL;
    current_cpu->current_thread->sleep.thread = current_cpu->current_thread;

    // Prepare
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
    while (t) {
        if (amounts != 0 && amnt >= amounts) break;
        
        // Wakeup the thread
        sleep_wakeup(t->thread);

        amnt++;
        t = t->next;
    }
    spinlock_release(&queue->lock);

    return amnt;
}

/**
 * @brief Change your mind and unprepare this thread for sleep
 * @param thread The thread to unprepare from sleep
 * @returns 0 on success
 */
int sleep_exit(thread_t *thr) {
    STUB();
}

/**
 * @brief Check if you are currently ready to sleep
 */
int sleep_isSleeping() {
    STUB(); // shouldn't be using this anyway
}