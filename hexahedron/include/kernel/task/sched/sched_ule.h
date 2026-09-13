/**
 * @file hexahedron/include/kernel/task/sched/sched_ule.h
 * @brief ULE scheduler
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#ifndef KERNEL_TASK_SCHEDULER_SCHED_ULE_H
#define KERNEL_TASK_SCHEDULER_SCHED_ULE_H

/**** INCLUDES ****/
#include <kernel/subsystems/timer.h>
#include <structs/bitmap.h>
#include <stdint.h>

/**** DEFINITIONS ****/

#define SCHED_NUM_QUEUES        64

#define SCHED_CLASS_KERNEL      0   // 0 - 64
#define SCHED_CLASS_REALTIME    1   // 64 - 128
#define SCHED_CLASS_TIMESHARE   2   // 128 - 224
#define SCHED_CLASS_IDLE        3   // 224 - 255

#define SCHED_PRIO_KERN_MIN     0
#define SCHED_PRIO_KERN_MAX     63
#define SCHED_PRIO_RT_MIN       64
#define SCHED_PRIO_RT_MAX       127
#define SCHED_PRIO_TS_MIN       128
#define SCHED_PRIO_TS_MAX       223
#define SCHED_PRIO_IDLE_MIN     224
#define SCHED_PRIO_IDLE_MAX     255

#define SCHED_INTERACT_MIN          0
#define SCHED_INTERACT_MAX          100
#define SCHED_INTERACT_HALF         (SCHED_INTERACT_MAX/2)
#define SCHED_INTERACT_THRESHOLD    30

#define SCHED_INTERACT_PRIO_MIN     0
#define SCHED_INTERACT_PRIO_MAX     63
#define SCHED_INTERACT_BATCH_MIN    64
#define SCHED_INTERACT_BATCH_MAX    95

#define SCHED_SLEEP_BONUS           16
#define SCHED_WAKEUP_BONUS          16

#define SCHED_SLP_RUN_MAX           (5ULL * 1000000000ULL)

// used on sched_ule_balance to keep threads lightly bound to their original CPU
#define SCHED_LOAD_THRESH           3

// is interactable?
#define SCHED_IS_INTERACT(uthr) ((uthr)->prio < 192)

// normalize timeshare queue index
#define SCHED_TS_NORMALIZE(val) (((val) + SCHED_NUM_QUEUES) % SCHED_NUM_QUEUES)
#define SCHED_TS_DISTANCE(from, to) SCHED_TS_NORMALIZE((to) - (from))

#define SCHED_TS_OFFSET(prio) ((unsigned)((prio - 192) * (SCHED_NUM_QUEUES - 1)) / (128))

/**** TYPES ****/

struct thread;

struct sched_ule_rq_entry {
    struct thread *head;
    struct thread *tail;
};

typedef struct sched_ule_rq {
    struct sched_ule_rq_entry queues[SCHED_NUM_QUEUES];
    BITMAP_DEFINE(status, SCHED_NUM_QUEUES);
} sched_ule_rq_t;

typedef struct sched_ule_ts {
    struct sched_ule_rq_entry queues[SCHED_NUM_QUEUES];
    BITMAP_DEFINE(status, SCHED_NUM_QUEUES);
    unsigned long idx;      // insertion index
    unsigned long ridx;     // removal index
} sched_ule_ts_t;

typedef struct sched_ule_cpu {
    spinlock_t lock;
    int threads;
    timer_event_t reschedule;
    timer_event_t calendar;

    sched_ule_rq_t rt_queue;
    sched_ule_ts_t ts_queue;
    sched_ule_rq_t ts_interact_queue;
    sched_ule_rq_t idle_queue;
} sched_ule_cpu_t;

typedef struct sched_ule_thread {
    struct thread *next;
    struct thread *prev;

    // times for the threads
    unsigned long sleep_time;
    unsigned long run_time;
    unsigned long run_start;
    unsigned long sleep_start;

    // priority and class
    unsigned char class;
    unsigned char prio;
    unsigned char sleep_boost;
} sched_ule_thread_t;

#endif
