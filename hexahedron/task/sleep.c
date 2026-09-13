/**
 * @file hexahedron/task/sleep.c
 * @brief Thread blocker/sleeper handler
 * 
 * @warning Time sleeping here sucks
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
#include <kernel/subsystems/timer.h>
#include <kernel/mm/alloc.h>
#include <kernel/panic.h>
#include <structs/list.h>
#include <kernel/debug.h>
#include <kernel/misc/util.h>
#include <string.h>
#include <assert.h>

/* Time lock */
spinlock_t time_lock = { 0 };

/* Time queue timer */
#define SLEEP_TIMER_INTERVAL 1000000
static spinlock_t sleep_timer_lock = { 0 };
static timer_event_t sleep_timer;
static bool sleep_timer_started = false;

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
 */
static void sleep_callback() {
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
        if (n->sl != current_cpu->current_thread && (seconds > n->seconds || (seconds == n->seconds && subseconds >= n->subseconds))) {
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
 * @brief Start the time queue timer
 */
static void sleep_startTimer() {
    spinlock_acquire(&sleep_timer_lock);

    if (sleep_timer_started == false) {
        timer_init(&sleep_timer, (timer_expire_t)sleep_callback, NULL, SLEEP_TIMER_INTERVAL, true, "thread sleep");
        timer_insert(&sleep_timer);
        sleep_timer_started = true;
    }

    spinlock_release(&sleep_timer_lock);
}

/**
 * @brief Put the current thread to sleep (while having X interrupt state)
 * @param state The IRQ state to restore exiting sleep_enter
 * 
 * This is used if you need to hold an IRQ lock while running sleep_prepare.
 */
void sleep_prepareIRQ(int state) {
    thread_t *cur = current_cpu->current_thread;

    if (IN_TASKLET()) {
        BUG("Sleeping on a tasklet is forbidden.");
    }

    hal_setInterruptState(HAL_INTERRUPTS_DISABLED);
    spinlock_acquireRaw(&cur->sleep.lock);
    cur->sleep.irq_state = state;
    cur->sleep.seconds = 0;
    cur->sleep.subseconds = 0;
    cur->sleep.queue = NULL;
    cur->sleep.interruptible = true;
    __sync_or_and_fetch(&current_cpu->current_thread->status, THREAD_STATUS_SLEEPING);
}

/**
 * @brief Put the current thread to sleep
 * 
 * Another thread will wake you up with @c sleep_wakeup
 * Use @c sleep_enter to actually enter the sleep state, which will also return the reason you woke up
 */
void sleep_prepare() {
    thread_t *cur = current_cpu->current_thread;

    if (IN_TASKLET()) {
        BUG("Sleeping on a tasklet is forbidden.");
    }

    int state = hal_setInterruptState(HAL_INTERRUPTS_DISABLED);
    spinlock_acquireRaw(&cur->sleep.lock);
    cur->sleep.irq_state = state;
    cur->sleep.seconds = 0;
    cur->sleep.subseconds = 0;
    cur->sleep.queue = NULL;
    cur->sleep.interruptible = true;
    __sync_or_and_fetch(&current_cpu->current_thread->status, THREAD_STATUS_SLEEPING);
}

/**
 * @brief Prepare thread for uninterruptible sleep
 * 
 * Signals will not be able to wake this thread
 */
void sleep_prepareUninterruptible() {
    thread_t *cur = current_cpu->current_thread;

    if (IN_TASKLET()) {
        BUG("Sleeping on a tasklet is forbidden.");
    }

    int state = hal_setInterruptState(HAL_INTERRUPTS_DISABLED);
    spinlock_acquireRaw(&cur->sleep.lock);
    cur->sleep.irq_state = state;
    cur->sleep.seconds = 0;
    cur->sleep.subseconds = 0;
    cur->sleep.queue = NULL;
    cur->sleep.interruptible = false;
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
    sleep_startTimer();
    sleep_prepare();
    
    // !!!: As a bit of backstory, I'm about halfway done with the VM rewrite. I have already rewritten the entire sleep system. I do NOT care enough to make this look good right now.
    current_cpu->current_thread->sleep.seconds = seconds;
    current_cpu->current_thread->sleep.subseconds = subseconds;
}

/**
 * @brief Remove a thread from a sleep queue
 */
static void sleep_unlinkFromQueue(sleep_queue_t *queue, thread_sleep_t *node) {
    thread_sleep_t *prev = (node->prev == node) ? NULL : node->prev;
    thread_sleep_t *next = (node->next == node) ? NULL : node->next;

    if (prev) {
        prev->next = next;
    } else if (queue->head == node) {
        queue->head = next;
    }

    if (next) next->prev = prev;

    node->next = NULL;
    node->prev = NULL;
}

/**
 * @brief Complete a wakeup (holding thread sleep lock)
 */
static int sleep_finishWakeup(thread_t *thread, int reason) {
    if (reason == WAKEUP_SIGNAL && thread->sleep.interruptible == false) {
        return 1;
    }

    if ((thread->status & THREAD_STATUS_SLEEPING) == 0) {
        return 1;
    }

    thread->sleep.queue = NULL;
    __sync_and_and_fetch(&thread->status, ~(THREAD_STATUS_SLEEPING));
    __atomic_store_n(&thread->sleep.wakeup_reason, reason, __ATOMIC_SEQ_CST);
    return 0;
}

/**
 * @brief Wakeup another thread for a reason
 * @param thread The thread to wakeup
 * @param reason The reason to wake the thread up
 */
int sleep_wakeupReason(struct thread *thread, int reason) {
    spinlock_acquire(&thread->sleep.lock);

    sleep_queue_t *queue = thread->sleep.queue;
    bool timed = thread->sleep.seconds || thread->sleep.subseconds;
    int r = sleep_finishWakeup(thread, reason);
    spinlock_release(&thread->sleep.lock);

    if (r != 0) {
        return r;
    }

    if (reason != WAKEUP_TIME && timed) {
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

    if (queue) {
        spinlock_acquire(&queue->lock);
        thread_sleep_t *node = queue->head;
        while (node && node != &thread->sleep) node = node->next;
        if (node) sleep_unlinkFromQueue(queue, node);
        spinlock_release(&queue->lock);
    }

    sched_event(thread, SCHED_EVENT_SLEEP_WAKEUP);
    sched_insert(thread);
    return 0;
}

/**
 * @brief Immediately trigger an early wakeup on a thread
 * @param thread The thread to wake up
 * @returns 0 on success
 */
inline int sleep_wakeup(struct thread *thread) {
    return sleep_wakeupReason(thread, WAKEUP_ANOTHER_THREAD);
}

/**
 * @brief Enter sleeping state now
 * @returns A sleep wakeup reason
 */
int sleep_enter() {
    thread_t *thread = current_cpu->current_thread;
    if (thread->sleep.seconds || thread->sleep.subseconds) {
        // We know the drill...
        // !!!: A full time rewrite is necessitated
        unsigned long seconds, subseconds;
        clock_getCurrentTime(&seconds, &subseconds);

        seconds += thread->sleep.seconds;
        subseconds += thread->sleep.subseconds;
        if (subseconds >= SUBSECONDS_PER_SECOND) {
            seconds += subseconds / SUBSECONDS_PER_SECOND;
            subseconds %= SUBSECONDS_PER_SECOND;
        }

        spinlock_acquire(&time_lock);
        struct internal_time_queue_entry ent = {
            .next = NULL,
            .sl = thread,
            .seconds = seconds,
            .subseconds = subseconds,
        };
        
        struct internal_time_queue_entry *n = head;
        while (n->next) n = n->next;
        n->next = &ent;
        spinlock_release(&time_lock);
    }

    if (hal_getInterruptState() != HAL_INTERRUPTS_DISABLED) {
        BUG("This thread entered sleep_enter with a different IRQ state than expected.\n\n"
            "It is illegal to enter sleep_enter with IRQs enabled.\n"
            "This can be caused by holding a lock while calling sleep_prepare and releasing it before enter.\n"
            "Use the API function for sleep_prepareIRQ if this is necessary."
        );
    }

    // Enter sleep
    timemonitor_updateSleepEnter();    
    
    sched_event(current_cpu->current_thread, SCHED_EVENT_SLEEP_ENTER);
    process_yield(0);

    // When exiting, the IRQ state was saved
    hal_setInterruptState(current_cpu->current_thread->sleep.irq_state);

    // Accumulate thread times
    timemonitor_updateSleepExit();
    
    // Clear seconds and subseconds
    current_cpu->current_thread->sleep.seconds = current_cpu->current_thread->sleep.subseconds = 0;

    if (__atomic_load_n(&current_cpu->current_thread->status, __ATOMIC_SEQ_CST) & THREAD_STATUS_STOPPING) {
        thread_exit();
        __builtin_unreachable();
    }

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
    int state = hal_setInterruptState(HAL_INTERRUPTS_DISABLED);
    spinlock_acquireRaw(&queue->lock);

    // prime the thread
    current_cpu->current_thread->sleep.next = NULL;
    current_cpu->current_thread->sleep.prev = NULL;
    current_cpu->current_thread->sleep.thread = current_cpu->current_thread;

    // Place ourselves in the queue
    if (queue->head) {
        thread_sleep_t *s = queue->head;
        while (s->next) s = s->next;
        s->next = &(current_cpu->current_thread->sleep);
        current_cpu->current_thread->sleep.prev = s;
    } else {
        queue->head = &current_cpu->current_thread->sleep;
        current_cpu->current_thread->sleep.prev = NULL;
    }

    // Prepare the thread to sleep
    sleep_prepareIRQ(state);
    current_cpu->current_thread->sleep.queue = queue;

    spinlock_releaseRaw(&queue->lock);
    return 0;
}

/**
 * @brief Wakeup threads in a sleep queue
 * @param queue The queue to start waking up
 * @param amount The amount of threads to wakeup. Non-positive wakes them all up
 * @returns Amount of threads awoken
 */
int sleep_wakeupQueue(sleep_queue_t *queue, int amounts) {
    int awoken = 0;
    while (1) {
        if (amounts > 0 && awoken >= amounts) {
            break;
        }

        spinlock_acquire(&queue->lock);
        thread_sleep_t *node = queue->head;
        if (node == NULL) {
            spinlock_release(&queue->lock);
            break;
        }

        thread_t *thread = node->thread;
        if (!spinlock_tryAcquire(&thread->sleep.lock)) {
            spinlock_release(&queue->lock);
            arch_pause_single();
            continue;
        }

        sleep_unlinkFromQueue(queue, node);

        // if a thread exited early, its queue will not match
        int r = 1;
        if (node->queue == queue) {
            r = sleep_finishWakeup(thread, WAKEUP_ANOTHER_THREAD);
            node->queue = NULL;
        }

        if (r == 0) {
            awoken++;
            spinlock_release(&thread->sleep.lock);
            spinlock_release(&queue->lock);
            sched_event(thread, SCHED_EVENT_SLEEP_WAKEUP);
            sched_insert(thread);
        } else {
            spinlock_release(&thread->sleep.lock);
            spinlock_release(&queue->lock);
        }
    }

    return awoken;
}

/**
 * @brief Change your mind and unprepare this thread for sleep
 * @returns 0 on success
 * @warning Usage of this is not recommended.
 */
int sleep_exit() {
    thread_t *thr = current_cpu->current_thread;

    // before the lock is released, snapshot the current queue
    sleep_queue_t *queue = thr->sleep.queue;
    thr->sleep.queue = NULL;

    // the thread can now be unmarked as sleeping
    __sync_and_and_fetch(&thr->status, ~(THREAD_STATUS_SLEEPING));

    // IRQs cannot be restored yet since the queue lock may need to be taken
    int state = thr->sleep.irq_state;
    spinlock_releaseRaw(&thr->sleep.lock);

    // hacky, unlink from queue
    if (queue) {
        spinlock_acquireRaw(&queue->lock);
        thread_sleep_t *node = queue->head;
        while (node && node != &thr->sleep) node = node->next;
        if (node) sleep_unlinkFromQueue(queue, node);
        spinlock_releaseRaw(&queue->lock);
    }

    hal_setInterruptState(state);

    return 0;
}
