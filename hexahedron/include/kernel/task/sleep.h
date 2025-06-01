/**
 * @file hexahedron/include/kernel/task/block.h
 * @brief Thread blocking/sleeping 
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_TASK_BLOCK_H
#define KERNEL_TASK_BLOCK_H

/**** INCLUDES ****/
#include <stdint.h>
#include <kernel/arch/arch.h>
#include <kernel/misc/spinlock.h>
#include <kernel/mem/mem.h>
#include <structs/node.h>

/**** DEFINITIONS ****/

/* Internal sleeping flags */
#define SLEEP_FLAG_NOCOND           0       // There is no condition under which the thread should wakeup. Dead thread. Should only be used for debugging.
#define SLEEP_FLAG_WAKEUP           1       // Whatever the case, wake it up NOW!
#define SLEEP_FLAG_TIME             2       // Thread is sleeping on time.
#define SLEEP_FLAG_COND             3       // Sleeping on condition

/* Reasons for waking up from sleep */
#define WAKEUP_SIGNAL               1       // A signal woke you up and you need to return -EINTR
#define WAKEUP_TIME                 2       // Timeout expired
#define WAKEUP_COND                 3       // Condition woke you up
#define WAKEUP_ANOTHER_THREAD       4       // Another thread woke up (also can be a queue)

/**** TYPES ****/

struct thread;

/**
 * @brief Sleep condition function
 * @param thread Thread to use for sleep condition
 * @param context Context provided by @c thread_blockXXX
 * @returns 0 on not ready to resume, 1 on ready to resume
 */
typedef int (*sleep_condition_t)(struct thread *thread, void *context);

/**
 * @brief Sleeper structure
 */
typedef struct thread_sleep {
    struct thread *thread;                  // Thread which is sleeping
    node_t *node;                           // Assigned node in the sleeping queue
    volatile int sleep_state;               // The current sleep state, and will also be set to wakeup reason

    // Conditional-sleeping threads
    sleep_condition_t condition;            // Condition on which to wakeup
    void *context;                          // Context for said condition

    // Specific to threads sleeping for time
    unsigned long seconds;                  // Seconds on which to wakeup
    unsigned long subseconds;               // Subseconds on which to wakeup
} thread_sleep_t;

/**
 * @brief Sleep queue structure
 * 
 * @note This is just a list object with a lock lmao 
 */
typedef struct sleep_queue {
    list_t queue;
    spinlock_t lock;
} sleep_queue_t;


/**** MACROS ****/

/* Put the entire process to sleep, including all of its threads */
#define SLEEP_ENTIRE_PROCESS(proc, sleep_run) { \
    struct thread *t = proc->main_thread; \
    sleep_run; \
    if (proc->thread_list) { \
        foreach(thread_node, proc->thread_list) { \
            t = (struct thread*)thread_node->value;\
            if (t) sleep_run; \
        } \
    } \
} \

/**** FUNCTIONS ****/

/**
 * @brief Initialize the sleeper system
 */
void sleep_init();

/**
 * @brief Put a thread to sleep, no condition and no way to wakeup without @c sleep_wakeup
 * @param thread The thread to sleep
 * @returns 0 on success
 * 
 * @note If you're putting the current thread to sleep, call @c sleep_enter right after
 */
int sleep_untilNever(struct thread *thread);

/**
 * @brief Put a thread to sleep until a specific condition is ready
 * @param thread The thread to put to sleep
 * @param condition Condition function
 * @param context Optional context passed to condition function
 * @returns 0 on success
 * 
 * @note If you're putting the current thread to sleep, call @c sleep_enter right after
 */
int sleep_untilCondition(struct thread *thread, sleep_condition_t condition, void *context);

/**
 * @brief Put a thread to sleep until a specific amount of time in the future has passed
 * @param thread The thread to put to sleep
 * @param seconds Seconds to wait in the future
 * @param subseconds Subseconds to wait in the future
 * @returns 0 on success
 * 
 * @note If you're putting the current thread to sleep, call @c sleep_enter right after
 */
int sleep_untilTime(struct thread *thread, unsigned long seconds, unsigned long subseconds);

/**
 * @brief Put a thread to sleep until a spinlock has unlocked
 * @param thread The thread to put to sleep
 * @param lock The lock to wait on
 * @returns 0 on success
 * 
 * @note If you're putting the current thread to sleep, call @c sleep_enter right after
 */
int sleep_untilUnlocked(struct thread *thread, spinlock_t *lock);

/**
 * @brief Immediately trigger an early wakeup on a thread
 * @param thread The thread to wake up
 * @returns 0 on success
 */
int sleep_wakeup(struct thread *thread);

/**
 * @brief Enter sleeping state now
 * @returns WAKEUP reason
 */
int sleep_enter();

/**
 * @brief Create a new sleep queue
 * @param name Optional name of the sleep queue
 * @returns Sleep queue object
 */
sleep_queue_t *sleep_createQueue(char *name);

/**
 * @brief Put yourself in a sleep queue
 * @param queue The queue to sleep in
 * @returns Whenever you get woken up. 0 means that you were woken up normally, 1 means you got interrupted
 */
int sleep_inQueue(sleep_queue_t *queue);

/**
 * @brief Wakeup threads in a sleep queue
 * @param queue The queue to start waking up
 * @param amount The amount of threads to wakeup. 0 wakes them all up
 * @returns Amount of threads awoken
 */
int sleep_wakeupQueue(sleep_queue_t *queue, int amounts);

/**
 * @brief Change your mind and unprepare this thread for sleep
 * @param thread The thread to unprepare from sleep
 * @returns 0 on success
 */
int sleep_exit(struct thread *thr);

#endif