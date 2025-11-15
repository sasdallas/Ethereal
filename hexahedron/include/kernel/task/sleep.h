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
#include <structs/node.h>
#include <structs/list.h>

/**** DEFINITIONS ****/

/* Reasons for waking up from sleep */
#define WAKEUP_SIGNAL               0       // A signal woke you up (interruption)
#define WAKEUP_TIME                 1       // Timeout expired
#define WAKEUP_ANOTHER_THREAD       2       // Another thread woke up

/**** TYPES ****/

struct thread;

/**
 * @brief Sleeper structure
 */
typedef struct thread_sleep {
    struct thread *thread;                  // Used for queues only
    struct thread_sleep *next;              // Used for queues only
    spinlock_t lock;                        // Used to prevent modifications to the thread's sleep state until release
    volatile int wakeup_reason;             // Reason this thread got woken up
    unsigned long seconds, subseconds;      // Seconds and subseconds
} thread_sleep_t;

/**
 * @brief Sleep queue structure
 * 
 * @note This is just a list object with a lock lmao 
 */
typedef struct sleep_queue {
    spinlock_t lock;
    thread_sleep_t *head;                   // Head of the list
} sleep_queue_t;


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
 * @brief Put the current thread to sleep
 * 
 * Another thread will wake you up with @c sleep_wakeup
 * Use @c sleep_enter to actually enter the sleep state, which will also return the reason you woke up
 */
void sleep_prepare();

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
 * @brief Check if you are currently ready to sleep
 */
int sleep_isSleeping();

/**
 * @brief Wakeup another thread for a reason
 * @param thread The thread to wakeup
 * @param reason The reason to wake the thread up
 */
int sleep_wakeupReason(struct thread *thread, int reason);

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
 * @returns 0 on success. Use sleep_enter to put yourself to sleep
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
void sleep_time(unsigned long seconds, unsigned long subseconds);

#endif