/**
 * @file hexahedron/task/itimer.c
 * @brief itimer manager
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#include <kernel/task/itimer.h>
#include <kernel/subsystems/timer.h>
#include <kernel/task/process.h>
#include <kernel/tasklet.h>

/**
 * @brief itimer expiration
 */
static void itimer_expire(void *context) {
    itimer_t *itimer = (itimer_t*)context;
    signal_send(itimer->proc, itimer->signal);

    time_t ns_intv = __atomic_load_n(&itimer->ns_intv, __ATOMIC_ACQUIRE);
    if (ns_intv != 0) {
        itimer->timer.intv = NS_TO_TICKS(ns_intv, current_cpu->timer_dev);
        if (itimer->timer.intv == 0) itimer->timer.intv = 1;
        timer_insert(&itimer->timer);
    }
}

/**
 * @brief Initialize an itimer for a process
 * @param itimer The itimer to initialize
 * @param proc The process to signal
 * @param signal Signal to deliver upon timer
 */
void itimer_init(itimer_t *itimer, struct process *proc, int signal) {
    itimer->signal = signal;
    itimer->proc = proc;
    itimer->ns_intv = 0;
    MUTEX_INIT(&itimer->lock);

    itimer->timer.active = false;
}

/**
 * @brief Set itimer
 * @param itimer The itimer to set
 * @param it_value Input is what to set, output is what was set
 * @param it_interval Ditto
 */
void itimer_set(itimer_t *itimer, struct timeval *it_value, struct timeval *it_interval) {
    mutex_acquire(&itimer->lock);
    time_t ns_val_old = 0;
    time_t ns_intv_old = __atomic_load_n(&itimer->ns_intv, __ATOMIC_ACQUIRE);

    // timer_remove is safe for timers that haven't been queued
    timer_remove(&itimer->timer, &ns_val_old);

    // TODO We could use the timer's repeat here, but that would mean I would have to update timer_init()
    //      to accept different intervals.
    time_t ns_intv = (it_interval->tv_sec * 1000000000) + (it_interval->tv_usec * 1000);
    __atomic_store_n(&itimer->ns_intv, ns_intv, __ATOMIC_RELEASE);

    if (it_value->tv_sec == 0 && it_value->tv_usec == 0) {
        goto _finish;
    }

    time_t ns_val = (it_value->tv_sec * 1000000000) + (it_value->tv_usec * 1000);
    
    timer_init(&itimer->timer, itimer_expire, itimer, ns_val, false, "proc_itimer");
    timer_insert(&itimer->timer);

_finish:
    if (ns_val_old) {
        it_value->tv_sec = ns_val_old / 1000000000;
        it_value->tv_usec = (ns_val_old % 1000000000) / 1000;
    } else {
        it_value->tv_sec = it_value->tv_usec = 0;
    }

    if (ns_intv_old) {
        it_interval->tv_sec = ns_intv_old / 1000000000;
        it_interval->tv_usec = (ns_intv_old % 1000000000) / 1000;
    } else {
        it_interval->tv_sec = it_interval->tv_usec = 0;
    }

    mutex_release(&itimer->lock);
}

/**
 * @brief Get itimer
 * @param itimer The itimer to get
 * @param it_value The value to get the itimer for
 * @param it_interval Repeat value
 */
void itimer_get(itimer_t *itimer, struct timeval *it_value, struct timeval *it_interval) {
    mutex_acquire(&itimer->lock);
    time_t ns_intv = __atomic_load_n(&itimer->ns_intv, __ATOMIC_ACQUIRE);

    time_t ns_rem = 0;
    int removed = timer_remove(&itimer->timer, &ns_rem);
    if (removed == 0) {
        // Requeue, moves it to this CPU.
        timer_init(&itimer->timer, itimer_expire, itimer, ns_rem > 0 ? ns_rem : 1000, false, "proc_itimer");
        timer_insert(&itimer->timer);
    }

    if (ns_rem != 0) {
        it_value->tv_sec = ns_rem / 1000000000;
        it_value->tv_usec = (ns_rem % 1000000000) / 1000;
    } else {
        it_value->tv_sec = it_value->tv_usec = 0;
    }

    if (ns_intv != 0) {
        it_interval->tv_sec = ns_intv / 1000000000;
        it_interval->tv_usec = (ns_intv % 1000000000) / 1000;
    } else {
        it_interval->tv_sec = it_interval->tv_usec = 0;
    }

    mutex_release(&itimer->lock);
}
