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
#include <kernel/panic.h>
#include <kernel/processor_data.h>
#include <kernel/arch/arch.h>
#include <stdbool.h>
#include <string.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "TASKLET", __VA_ARGS__)

/**
 * @brief Unlink a tasklet while holding the queue's lock
 */
static void tasklet_unlinkLocked(tasklet_t *tasklet) {
    if (tasklet->active == false) {
        return;
    }

    tasklet_cpu_t *owner = tasklet->owner;

    if (tasklet->prev) {
        tasklet->prev->next = tasklet->next;
    } else {
        owner->queue = tasklet->next;
    }

    if (tasklet->next) tasklet->next->prev = tasklet->prev;

    owner->pending--;
 
    tasklet->next = tasklet->prev = NULL;
    tasklet->active = false;
 
    if (!tasklet->running) {
        __atomic_store_n(&tasklet->owner, NULL, __ATOMIC_RELEASE);
    }
}

/**
 * @brief Initialize tasklet system (per-CPU)
 */
void tasklet_init() {
    current_cpu->tasklet = kzalloc(sizeof(tasklet_cpu_t));
    SPINLOCK_INIT(&current_cpu->tasklet->lock);
}

/**
 * @brief Process pending tasklets on current CPU
 * Called from IRQ context with interrupts disabled
 */
void tasklet_process() {
    if (IN_TASKLET()) return; // cannot process tasklets in tasklets
    
    TASKLET_ENTER();
    tasklet_cpu_t *tsk = current_cpu->tasklet;

    for (;;) {
        spinlock_acquire(&tsk->lock);

        // check if we even have a tasklet
        if (current_cpu->tasklet->queue == NULL) {
            spinlock_release(&tsk->lock);
            break;
        }

        tasklet_t *ts = current_cpu->tasklet->queue;
        __atomic_store_n(&ts->running, true, __ATOMIC_RELEASE);

        tasklet_unlinkLocked(ts);
        current_cpu->tasklet->executing = ts;

        // capture ctx and cb, as once we release the lock we lose a lot of control
        void *ctx = ts->tasklet_ctx;
        tasklet_cb_t cb = ts->cb;
        spinlock_release(&tsk->lock);

        // Execute the tasklet with IRQs enabled
        int irq_state = hal_setInterruptState(HAL_INTERRUPTS_ENABLED);

        if (UNLIKELY(cb == NULL)) {
            hal_setInterruptState(irq_state);
            BUG("Queued tasklet %p (%s) has no callback", ts, ts->name);
        }

        cb(ctx);

        hal_setInterruptState(irq_state);

        // Done executing, now we can relock the tasklet queue and finish delinking it.
        spinlock_acquire(&tsk->lock);
        current_cpu->tasklet->executing = NULL;
        __atomic_store_n(&ts->running, false, __ATOMIC_RELEASE);

        // Possible this tasklet may have been re-queued during its callback
        if (!ts->active) {
            __atomic_store_n(&ts->owner, NULL, __ATOMIC_RELEASE);
        }

        spinlock_release(&tsk->lock);
    }

    TASKLET_EXIT();
}

/**
 * @brief Insert a tasklet onto the current CPU queue
 * @param tasklet The tasklet to insert
 */
void tasklet_insert(tasklet_t *tasklet) {
    // LOG(DEBUG, "Priming tasklet \"%s\"\n", tasklet->name);
    tasklet_cpu_t *tsk = current_cpu->tasklet;

    spinlock_acquire(&tsk->lock);
    tasklet_cpu_t *owner = NULL;

    if (!__atomic_compare_exchange_n(&tasklet->owner, &owner, tsk, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE) && owner != tsk) {
        spinlock_release(&tsk->lock);
        return;
    }

    if (tasklet->active) {
        spinlock_release(&tsk->lock);
        return;
    }

    tasklet->next = tsk->queue;
    if (tsk->queue) tsk->queue->prev = tasklet;
    tsk->queue = tasklet;
    tasklet->prev = NULL;
    tasklet->active = true;

    tsk->pending += 1;

    spinlock_release(&tsk->lock);
}

/**
 * @brief Race conditions can be defeated if you have enough retries
 * 
 * On a serious note this locks the owner of a tasklet by racing it to make @c tasklet_remove safe
 * If it returns NULL this tasklet is unowned and safe to use
 */
static tasklet_cpu_t *tasklet_lockOwner(tasklet_t *tasklet) {
    for (;;) {
        tasklet_cpu_t *owner = __atomic_load_n(&tasklet->owner, __ATOMIC_ACQUIRE);
        if (!owner) return NULL;

        spinlock_acquire(&owner->lock);
        if (__atomic_load_n(&tasklet->owner, __ATOMIC_ACQUIRE) == owner) {
            return owner;
        }
        spinlock_release(&owner->lock);
    }
}

/**
 * @brief Remove a queued tasklet from its owner CPU
 * @param tasklet The tasklet to remove
 */
void tasklet_remove(tasklet_t *tasklet) {
    tasklet_cpu_t *owner = tasklet_lockOwner(tasklet);
    if (!owner) return;

    tasklet_unlinkLocked(tasklet);
    spinlock_release(&owner->lock);
}

/**
 * @brief This is a helper function to remove a tasklet from its executor
 * Upon completion this tasklet will be guranteed not queued
 * 
 */
void tasklet_drain(tasklet_t *tasklet) {
    for (;;) {
        tasklet_cpu_t *owner = tasklet_lockOwner(tasklet);
        if (!owner) return;

        tasklet_unlinkLocked(tasklet);

        bool running = tasklet->running;
        bool self = owner == current_cpu->tasklet && owner->executing == tasklet;

        spinlock_release(&owner->lock);

        // if it's running on ourselves, its clearly not anymore
        if (!running || self) {
            return;
        }

        arch_pause_single();
    }
}
