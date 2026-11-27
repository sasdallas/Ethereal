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
struct tasklet_cpu;
typedef void (*tasklet_cb_t)(void *context);

typedef struct tasklet {
    struct tasklet *next;
    struct tasklet *prev;

    bool active;
    bool running;
    struct tasklet_cpu *owner;

    char *name;
    tasklet_cb_t cb;
    void *tasklet_ctx;
} tasklet_t;

typedef struct tasklet_cpu {
    spinlock_t lock;
    tasklet_t *queue;
    tasklet_t *executing;
    int pending;
} tasklet_cpu_t;

/**** MACROS ****/

#define TASKLET_DECLARE(_vname, _name, _cb, _ctx) tasklet_t _vname = { .next = NULL, .prev = NULL, .name = _name, .cb = _cb, .tasklet_ctx = _ctx, .active = false, .running = false, .owner = NULL }
#define TASKLET_INIT(t, _name, _cb, _ctx) ({ (t)->name = _name; (t)->cb = _cb; (t)->tasklet_ctx = _ctx; (t)->next = (t)->prev = NULL; (t)->active = false; (t)->running = false; (t)->owner = NULL; })

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
 * @brief Remove a queued tasklet from its owner CPU
 * @param tasklet The tasklet to remove
 */
void tasklet_remove(tasklet_t *tasklet);

/** Remove a queued tasklet and wait for an executing callback to finish. */
void tasklet_drain(tasklet_t *tasklet);

#endif
