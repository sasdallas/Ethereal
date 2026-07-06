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
#include <kernel/debug.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "TIMER", __VA_ARGS__)

#define NS_TO_TICKS(ns, dev) (((ns) * (dev)->freq.mult) >> (dev)->freq.shift)

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
 * @brief Repeat tasklet
 */
static void timer_repeatTasklet(void *context) {
    timer_device_t *dev = (timer_device_t*)context;
    
    // TODO: profile
    spinlock_acquire(&dev->queue_lock);
    timer_stop();

    timer_event_t *ev = dev->pending_repeats;
    dev->pending_repeats = NULL;

    while (ev) {
        timer_event_t *nxt_save = ev->next;
        timer_insertLocked(ev);
        ev = nxt_save;
    }

    timer_arm();
    spinlock_release(&dev->queue_lock);
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

    // while we can keep executing
    bool do_repeats = false;
    timer_event_t *ev = device->queue;
    while (ev && ev->absolute_expire <= device->total_ticks) {
        // execute event
        timer_event_t *next_ev = ev->next;
        device->queue = next_ev;
        ev->next = ev->prev = NULL;
        tasklet_insert(&ev->tsklet);

        if (ev->repeat) {
            // Insert into repeats, ignore previous pointer since that's not used in repeat code
            // I couldn't care less about missed deadlines that's the tasklet system's fault 
            // !!!: this is done backwards which shouldnt really matter but might cause a bit of latency
            // ev->next = device->pending_repeats;
            // device->pending_repeats = ev;
            
            // we need to do repeats
            // do_repeats = true;

            timer_insertLocked(ev);
        } else {
            ev->active = false;
        }

        ev = next_ev;
    }

    // cleanup the linked list
    if (device->queue) device->queue->prev = NULL;


    // the repeat mechanism builds a small list reusing the timer pointers
    // then uses a tasklet to re-insert them asyncronously
    if (do_repeats && device->repeat_tasklet.active == false) {
        tasklet_insert(&device->repeat_tasklet);
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
    timer->intv = NS_TO_TICKS(ns, current_cpu->timer_dev);
    timer->active = false;
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

    // Re-arm timer if needed
    timer_arm();

    spinlock_release(&dev->queue_lock);
    return ret;
}

/**
 * @brief Remove a timer entry
 * @param timer The timer to remove
 */
int timer_remove(timer_event_t *timer) {
    // acquire queue lock and stop current timer
    timer_device_t *dev = current_cpu->timer_dev;
    spinlock_acquire(&dev->queue_lock);

    // stop timer and update ticks
    timer_stop();

    if (timer->active == false) {
        // This timer is not active
        timer_arm();
        spinlock_release(&dev->queue_lock);
        return 1;
    }

    if (timer == dev->queue) {
        // the timer is the head of the queue
        dev->queue = timer->next;
        if (dev->queue) dev->queue->prev = NULL;
    } else {
        if (timer->prev) timer->prev->next = timer->next;
        if (timer->next) timer->next->prev = timer->prev;
    }

    timer->active = false;

    // Re-arm timer
    timer_arm();

    spinlock_release(&dev->queue_lock);
    return 0;
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
    TASKLET_INIT(&dev->repeat_tasklet, "timer repeat", timer_repeatTasklet, dev);
    dev->private = private;
    return dev;
}
