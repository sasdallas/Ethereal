/**
 * @file hexahedron/include/kernel/tasklet.h
 * @brief Tasklet (DPC ) runtime
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef KERNEL_TASKLET_H
#define KERNEL_TASKLET_H

/**** INCLUDES ****/
#include <stdint.h>
#include <stdbool.h>
#include <kernel/misc/spinlock.h>

/**** DEFINITIONS ****/

/**** TYPES ****/

struct tasklet;
typedef void (*tasklet_cb_t)(void *context);

typedef struct tasklet {
    struct tasklet *next;
    struct tasklet *prev;

    bool active;
    char *name;
    tasklet_cb_t cb;
    void *tasklet_ctx;
} tasklet_t;

typedef struct tasklet_cpu {
    spinlock_t lock;
    tasklet_t *queue;
    int pending;
} tasklet_cpu_t;

/**** MACROS ****/

#define TASKLET_DECLARE(_vname, _name, _cb, _ctx) tasklet_t _vname = { .next = NULL, .prev = NULL, .name = _name, .cb = _cb, .tasklet_ctx = _ctx }
#define TASKLET_INIT(t, _name, _cb, _ctx) ({ (t)->name = _name; (t)->cb = _cb; (t)->tasklet_ctx = _ctx; (t)->next = (t)->prev = NULL; })

/**** FUNCTIONS ****/

/**
 * @brief Initialize tasklet system (per-CPU)
 */
void tasklet_init();

/**
 * @brief Process pending tasklets on current CPU
 */
void tasklet_process();

/**
 * @brief Insert a tasklet onto the current CPU queue
 * @param tasklet The tasklet to insert
 */
void tasklet_insert(tasklet_t *tasklet);

/**
 * @brief Remove a tasklet from the current CPU queue
 * @param tasklet The tasklet to remove from the current CPU queue
 */
void tasklet_remove(tasklet_t *tasklet);

#endif
