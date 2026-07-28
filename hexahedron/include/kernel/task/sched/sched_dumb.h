/**
 * @file hexahedron/include/kernel/task/sched/sched_dumb.h
 * @brief Dumb scheduler that just pops threads
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#ifndef KERNEL_TASK_SCHED_SCHED_DUMB_H
#define KERNEL_TASK_SCHED_SCHED_DUMB_H

/**** INCLUDES ****/
#include <stdint.h>
#include <structs/list.h>
#include <kernel/misc/spinlock.h>
#include <kernel/subsystems/timer.h>

/**** TYPES ****/

typedef struct sched_dumb_queue {
    timer_event_t timer;
    spinlock_t lock;
    list_t queue;
} sched_dumb_queue_t;

#endif
