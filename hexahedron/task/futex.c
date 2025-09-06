/**
 * @file hexahedron/task/futex.c
 * @brief Futex support
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 * 
 * @note Some ideas derived from @Bananymous
 * @todo Check this for security issues. Messing with physical pointers seems like, well, a bad idea..
 */

#include <kernel/task/futex.h>
#include <kernel/misc/mutex.h>
#include <kernel/debug.h>
#include <structs/hashmap.h>
#include <kernel/mem/alloc.h>
#include <kernel/processor_data.h>
#include <errno.h>
#include <assert.h>
#include <string.h>

#define LOG(status, ...) dprintf_module(status, "TASK:FUTEX", __VA_ARGS__)

hashmap_t *futex_map = NULL;
mutex_t *futex_mutex = NULL;

/* !!!: Fuck this code */
void *__futex_avoid_tripping_sanity_check() {
    return __builtin_return_address(0);
}

/**
 * @brief Wait on a futex
 * @param pointer Pointer to the futex variable
 * @param value The vaue to look for
 * @param time Time to wait on
 */
int futex_wait(int *pointer, int val, const struct timespec *time) {
    if (__atomic_load_n(pointer, memory_order_seq_cst) != val) {
        return -EAGAIN;
    }

    // First let's get ourselves ready to sleep
    mutex_acquire(futex_mutex);

    uintptr_t ptr_phys = mem_getPhysicalAddress(NULL, (uintptr_t)pointer);

    futex_t *f = NULL;
    if (hashmap_has(futex_map, (void*)ptr_phys)) {
        f = hashmap_get(futex_map, (void*)ptr_phys);
    } else {
        // Create a new one
        f = kzalloc(sizeof(futex_t));
        f->queue = sleep_createQueue("futex queue");
        hashmap_set(futex_map, (void*)ptr_phys, f);
    }

    // sleep_inQueue(f->queue);
    
    while (1) {
        if (time) {
            // !!!: TIME OUT WILL NOT WORK IF SOME THREAD KEEPS WAKING US UP
            sleep_untilTime(current_cpu->current_thread, time->tv_sec, time->tv_nsec / 1000);
        } else {
            sleep_untilNever(current_cpu->current_thread);
        }

        current_cpu->current_thread->sleep->context = __futex_avoid_tripping_sanity_check();
        
        list_append(&f->queue->queue, (void*)current_cpu->current_thread);

        mutex_release(futex_mutex);
        
        int w = sleep_enter();
        if (w == WAKEUP_SIGNAL) { return -EINTR; }
        if (w == WAKEUP_TIME) { return -ETIMEDOUT; }
        assert(w == WAKEUP_ANOTHER_THREAD);

        mutex_acquire(futex_mutex);

        // ..?
        if (__atomic_load_n(pointer, memory_order_seq_cst) == val) {
            continue; // Try again
        }

        // Drop us if needed
        if (!f->queue->queue.length) {
            hashmap_remove(futex_map, (void*)ptr_phys);
            kfree(f->queue);
            kfree(f);
        }

        mutex_release(futex_mutex); 
        return 0;
    }
    
}

/**
 * @brief Wake up a futex
 * @param pointer Pointer to the futex
 */
int futex_wakeup(int *pointer) {
    mutex_acquire(futex_mutex);

    uintptr_t ptr_phys = mem_getPhysicalAddress(NULL, (uintptr_t)pointer);

    if (!hashmap_has(futex_map, (void*)ptr_phys)) {
        mutex_release(futex_mutex);
        return 0;
    }

    futex_t *f = (futex_t*)hashmap_get(futex_map, (void*)ptr_phys);
    sleep_wakeupQueue(f->queue, 1);

    mutex_release(futex_mutex);
    return 0;
}

/**
 * @brief Initialize futexes
 */
void futex_init() {
    futex_mutex = mutex_create("futex");
    futex_map = hashmap_create_int("futex map", 10);
}