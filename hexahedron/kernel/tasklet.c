/**
 * @file hexahedron/kernel/tasklet.c
 * @brief Kernel tasklet subsystem
 * 
 * Extremely simple. Processed during interrupt end.
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/tasklet.h>
#include <kernel/subsystems/irq.h>
#include <kernel/debug.h>
#include <kernel/processor_data.h>
#include <stdbool.h>
#include <string.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "TASKLET", __VA_ARGS__)

#define TASKLET_LOCK() int __state = hal_setInterruptState(HAL_INTERRUPTS_DISABLED)
#define TASKLET_UNLOCK() hal_setInterruptState(__state)

/**
 * @brief Initialize tasklet system (per-CPU)
 */
void tasklet_init() {
    current_cpu->tasklet = kzalloc(sizeof(tasklet_t));
}

/**
 * @brief Process pending tasklets on current CPU
 * Called from IRQ context
 */
void tasklet_process() {
    tasklet_t *ts = current_cpu->tasklet->queue;
    TASKLET_LOCK();
    while (ts) {
        // Delink from list
        if (ts->prev) ts->prev->next = ts->next;
        else current_cpu->tasklet->queue = ts->next;
        if (ts->next) ts->next->prev = ts->prev;
    
        tasklet_t *nxt_saved = ts->next;
        ts->prev = ts->next = NULL;
        ts->active = false;

        // Execute using this mess cause fuck readability
        TASKLET_ENTER(); // << prevents threads from preempting the tasklet
        
        TASKLET_UNLOCK();
        ts->cb(ts->tasklet_ctx);
        TASKLET_LOCK();
        
        TASKLET_EXIT();

        current_cpu->tasklet->pending -= 1;
        ts = nxt_saved;
    }
    TASKLET_UNLOCK();
}

/**
 * @brief Insert a tasklet onto the current CPU queue
 * @param tasklet The tasklet to insert
 */
void tasklet_insert(tasklet_t *tasklet) {
    // LOG(DEBUG, "Priming tasklet \"%s\"\n", tasklet->name);
    tasklet_cpu_t *tsk = current_cpu->tasklet;
    TASKLET_LOCK();
    if (tasklet->active) {
        TASKLET_UNLOCK();
        return;
    }

    tasklet->next = tsk->queue;
    tsk->queue = tasklet;
    tasklet->prev = NULL;
    tasklet->active = true;
    tsk->pending += 1;
    TASKLET_UNLOCK();
}

/**
 * @brief Remove a tasklet from the current CPU queue
 * @param tasklet The tasklet to remove from the current CPU queue
 */
void tasklet_remove(tasklet_t *tasklet) {
    TASKLET_LOCK();
    if (tasklet->prev) tasklet->prev->next = tasklet->next;
    else current_cpu->tasklet->queue = tasklet->next;
    if (tasklet->next) tasklet->next->prev = tasklet->prev;
    if (tasklet->active) current_cpu->tasklet->pending--;
    tasklet->active = false;
    TASKLET_UNLOCK();
}
