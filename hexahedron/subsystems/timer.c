/**
 * @file hexahedron/drivers/timer.c
 * @brief Timer system
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/subsystems/timer.h>
#include <kernel/processor_data.h>
#include <kernel/smp.h>
#include <kernel/debug.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "TIMER", __VA_ARGS__)

/* bs */
struct remove_ctx {
    timer_event_t *timer;
    bool was_queued;
    time_t rem_ns;
};

/**
 * @brief Arm the timer
 */
static void timer_arm() {
    timer_device_t *dev = current_cpu->timer_dev;
    if (dev->queue == NULL) return;
    dev->ops->set_timer(dev, dev->queue->absolute_expire - dev->total_ticks);
}

/**
 * @brief Insert a timer while locked
 * Also expects timer to be stopped
 */
static int timer_insertLocked(timer_event_t *timer) {
    timer_device_t *dev = current_cpu->timer_dev;

    // Now figure out the baseline for this timer
    timer->absolute_expire = dev->total_ticks + timer->intv;

    // Okay into the list you go
    if (dev->queue == NULL) {
        // First timer
        timer->next = NULL;
        timer->prev = NULL;
        dev->queue = timer;
        return 0;
    }

    if (timer->absolute_expire <= dev->queue->absolute_expire) {
        // We expire before the first timer
        timer->next = dev->queue;
        dev->queue->prev = timer;
        timer->prev = NULL;
        dev->queue = timer;
        return 0;
    }

    // Otherwise we have to go linked list searching
    // TODO: replace this with an rbtree
    timer_event_t *check = dev->queue;
    while (1) {
        if (check->next == NULL) {
            // Last timer in queue
            check->next = timer;
            timer->prev = check;
            timer->next = NULL;
            break;
        } else if (timer->absolute_expire <= check->next->absolute_expire) {
            timer->next = check->next;
            timer->prev = check;
            check->next->prev = timer;
            check->next = timer;
            break;
        }

        check = check->next;
    }

    return 0;
}

/**
 * @brief Stop the current timer and update ticks
 * expects lock held
 */
static void timer_stop() {
    timer_device_t *dev = current_cpu->timer_dev;
    dev->ops->stop(dev);

    // Update total_ticks
    if (dev->queue != NULL) {
        uint64_t elapsed = dev->ops->get_ticks_elapsed(dev);
        dev->total_ticks += elapsed;
    }
}


/**
 * @brief Timer IRQ callback
 */
void timer_irq() {
    timer_device_t *device = current_cpu->timer_dev;
    spinlock_acquire(&device->queue_lock);

    // shut up timer
    device->ops->stop(device);

    // the expiration indicates that the current queue head expired
    if (!device->queue) {
        // ok i dont know
        spinlock_release(&device->queue_lock);
        return;
    }

    // we can be sure of this
    device->total_ticks = device->queue->absolute_expire;

    timer_event_t *repeats = NULL;
    while (device->queue && device->queue->absolute_expire <= device->total_ticks) {
        timer_event_t *ev = device->queue;
        device->queue = ev->next;
        if (device->queue) device->queue->prev = NULL;
        ev->next = ev->prev = NULL;
        tasklet_insert(&ev->tsklet);

        if (ev->repeat) {
            ev->next = repeats;
            repeats = ev;
        } else {
            ev->active = false;
            __atomic_store_n(&ev->cpu, NULL, __ATOMIC_RELEASE);
        }
    }

    // finish cleaning up the repeat list
    while (repeats) {
        timer_event_t *ev = repeats;
        repeats = ev->next;
        ev->next = NULL;
        timer_insertLocked(ev);
    }

    // re-arm the timer if needed
    timer_arm();

    spinlock_release(&device->queue_lock);
}

/**
 * @brief Initialize a timer
 * @param timer The timer to initialize
 * @param expire Expire function
 * @param expire_ctx Expire context
 * @param ns Nanoseconds to expiration
 * @param repeat Whether timer should be repeated
 * @param name Optional timer name
 * @returns 0 on success
 */
int timer_init(timer_event_t *timer, timer_expire_t expire, void *expire_ctx, time_t ns, bool repeat, char *name) {
    TASKLET_INIT(&timer->tsklet, name, expire, expire_ctx);
    
    timer->repeat = repeat;
    // timer->intv = ((__uint128_t)ns * current_cpu->timer_dev->freq.mult) >> current_cpu->timer_dev->freq.shift;
    timer->intv = NS_TO_TICKS(ns, current_cpu->timer_dev);
    if (ns > 0 && timer->intv == 0) timer->intv = 1;
    timer->active = false;
    __atomic_store_n(&timer->cpu, NULL, __ATOMIC_RELEASE);
    return 0;
}


/**
 * @brief Insert and start timer
 * @param timer The timer to insert
 */
int timer_insert(timer_event_t *timer) {
    // acquire queue lock and stop current timer
    timer_device_t *dev = current_cpu->timer_dev;
    spinlock_acquire(&dev->queue_lock);

    // stop timer and update ticks
    timer_stop();

    int ret = timer_insertLocked(timer);
    timer->active = true;
    __atomic_store_n(&timer->cpu, GET_PROCESSOR(), __ATOMIC_RELEASE);

    // Re-arm timer if needed
    timer_arm();

    spinlock_release(&dev->queue_lock);
    return ret;
}

/**
 * @brief Remove inner
 */
static void timer_removeInner(void *context) {
    struct remove_ctx *remove = (struct remove_ctx*)context;
    timer_event_t *timer = remove->timer;

    // acquire queue lock and stop current timer
    timer_device_t *dev = current_cpu->timer_dev;
    spinlock_acquire(&dev->queue_lock);

    // timer_stop() will clear the countdown, so get it
    uint64_t remaining = 0;
    if (timer->active && __atomic_load_n(&timer->cpu, __ATOMIC_ACQUIRE) == GET_PROCESSOR() && dev->queue) {
        uint64_t head_remaining = dev->ops->get_ticks_remaining(dev);
        remaining = head_remaining + (timer->absolute_expire - dev->queue->absolute_expire);
    }

    // stop timer and update ticks
    timer_stop();

    if (!dev->queue || timer->active == false || __atomic_load_n(&timer->cpu, __ATOMIC_ACQUIRE) != GET_PROCESSOR()) {
        // This timer is not active or does not belong to us anymore
        timer_arm();
        spinlock_release(&dev->queue_lock);
        remove->was_queued = false;
        remove->rem_ns = 0;
        return;
    }

    remove->rem_ns = TICKS_TO_NS(remaining, dev);

    if (timer == dev->queue) {
        // the timer is the head of the queue
        dev->queue = timer->next;
        if (dev->queue) dev->queue->prev = NULL;
    } else {
        if (timer->prev) timer->prev->next = timer->next;
        if (timer->next) timer->next->prev = timer->prev;
    }

    timer->active = false;
    __atomic_store_n(&timer->cpu, NULL, __ATOMIC_RELEASE);
    remove->was_queued = true;

    // Re-arm timer
    timer_arm();

    spinlock_release(&dev->queue_lock);
}

/**
 * @brief Remove a timer entry
 * @param timer The timer to remove
 * @param ns_rem Optional pointer to store remaining nanoseconds
 * @returns Negative on error, 0 on success, 1 on wasn't queued
 */
int timer_remove(timer_event_t *timer, time_t *ns_rem) {
    if (ns_rem) *ns_rem = 0;

    // This is a loop because I observed that the timers can be requeued onto other CPUs
    // before timer_removeInner gets a chance to execute
    for (;;) {
        processor_t *timer_cpu = __atomic_load_n(&timer->cpu, __ATOMIC_ACQUIRE);
        if (!timer_cpu) {
            // Even if the CPU is nonexistent that could mean the tasklet is still executing
            // Let's wait for it just in case
            tasklet_drain(&timer->tsklet);

            if (__atomic_load_n(&timer->cpu, __ATOMIC_ACQUIRE)) {
                continue;
            }

            return 1;
        }

        struct remove_ctx ctx = { .timer = timer };
        if (GET_PROCESSOR() == timer_cpu) {
            timer_removeInner(&ctx);
        } else {
            procmask_t pmask = PROCMASK_INITIALIZER;
            procmask_set(&pmask, timer_cpu->cpu_id);

            int r = smp_callFunctionMask(&pmask, timer_removeInner, &ctx, true);
            if (r != 0) {
                LOG(ERR, "smp_callFunctionMask failed with error %d\n", r);
                return r;
            }
        }

        if (ctx.was_queued) {
            tasklet_drain(&timer->tsklet);
            if (__atomic_load_n(&timer->cpu, __ATOMIC_ACQUIRE)) continue;
            if (ns_rem) *ns_rem = ctx.rem_ns;
            return 0;
        }

        if (__atomic_load_n(&timer->cpu, __ATOMIC_ACQUIRE) == timer_cpu) {
            return 1;
        }
    }
}

/**
 * @brief Select the timer for this CPU
 * @param timer The timer to select for the CPU
 */
void timer_selectDevice(timer_device_t *device) {
    timer_device_t *cur = current_cpu->timer_dev;
    if (cur == NULL || cur->priority < device->priority) {
        if (cur) {
            cur->ops->deinit(cur);
        }

        current_cpu->timer_dev = device;
        device->ops->init(device);
        LOG(INFO, "Selected timer source \"%s\" with %llu tick per ns\n", device->name, NS_TO_TICKS(1, device));
    }
}

/**
 * @brief Create a timer device
 * @param name Name of the timer device
 * @param ops Operations
 * @param priority Priority of the timer device
 * @param private Timer-specific
 */
timer_device_t *timer_createDevice(char *name, timer_ops_t *ops, unsigned char priority, void *private) {
    timer_device_t *dev = kzalloc(sizeof(timer_device_t));
    dev->name = name;
    dev->priority = priority;
    dev->ops = ops;
    dev->private = private;
    return dev;
}
