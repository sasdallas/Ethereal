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

#include <kernel/task/process.h>
#include <kernel/misc/mutex.h>
#include <kernel/debug.h>
#include <structs/hashmap.h>
#include <kernel/mm/alloc.h>
#include <kernel/misc/util.h>
#include <kernel/panic.h>
#include <kernel/event.h>
#include <errno.h>
#include <assert.h>
#include <string.h>

#define LOG(status, ...) dprintf_module(status, "TASK:FUTEX", __VA_ARGS__)

slab_cache_t *futex_cache = NULL;
hashmap_t *futex_map = NULL;
mutex_t *futex_mutex = NULL;

/**
 * @brief Futex object initializer
 */
static int futex_initializer(slab_cache_t *cache, void *obj) {
    futex_t *f = obj;
    EVENT_INIT(&f->futex_event);
    f->waiters = 0;
    f->wakers = 0;
    return 0;
} 

/**
 * @brief Wait on a futex
 * @param pointer Pointer to the futex variable
 * @param value The vaue to look for
 * @param time Time to wait on
 */
int futex_wait(uint32_t *pointer, uint32_t val, const struct timespec *time) {
    if (__atomic_load_n(pointer, memory_order_seq_cst) != val) {
        return -EAGAIN;
    }

    uintptr_t phys = arch_mmu_physical(NULL, (uintptr_t)pointer);

    mutex_acquire(futex_mutex);

    futex_t *f = hashmap_get(futex_map, (void*)phys);
    if (!f) {
        f = slab_allocate(futex_cache);
        if (!f) { mutex_release(futex_mutex); return -ENOMEM; }
        hashmap_set(futex_map, (void*)phys, f);    
    }

    event_listener_t listener;
    EVENT_INIT_LISTENER(&listener);

    
    while (1) {
        EVENT_ATTACH(&listener, &f->futex_event);        
        f->waiters++;
        
        mutex_release(futex_mutex);

        int ret;
        if (time) {
            ret = EVENT_WAIT(&listener, time->tv_sec * 1000 + (time->tv_nsec / 1000));
        } else {
            ret = EVENT_WAIT(&listener, -1);
        }

        mutex_acquire(futex_mutex);

        EVENT_DETACH(&listener);

        if (ret < 0) {
            EVENT_DESTROY_LISTENER(&listener);
            f->waiters--;
            mutex_release(futex_mutex);
            return ret;
        }

        if (__atomic_load_n(pointer, memory_order_seq_cst) == val || f->wakers == 0) {
            continue;
        }

        f->waiters--;
        f->wakers--;
        break;
    }

    EVENT_DESTROY_LISTENER(&listener);
    if (f->waiters == 0) {
        hashmap_remove(futex_map, (void*)phys);
        slab_free(futex_cache, f);
    }

    mutex_release(futex_mutex);
    return 0;
}

/**
 * @brief Wake up a futex
 * @param pointer Pointer to the futex
 * @param val How many waiters to wakeup
 */
int futex_wakeup(uint32_t *pointer, uint32_t val) {
    mutex_acquire(futex_mutex);

    uintptr_t ptr_phys = arch_mmu_physical(NULL, (uintptr_t)pointer);

    if (!hashmap_has(futex_map, (void*)ptr_phys)) {
        mutex_release(futex_mutex);
        return 0;
    }

    futex_t *f = (futex_t*)hashmap_get(futex_map, (void*)ptr_phys);
    
    int to_wakeup = min(f->waiters - f->wakers, val);

    f->wakers += to_wakeup;
    
    EVENT_SIGNAL(&f->futex_event);
    mutex_release(futex_mutex);
    return 0;
}

/**
 * @brief Initialize futexes
 */
void futex_init() {
    futex_cache = slab_createCache("futex cache", SLAB_CACHE_DEFAULT, sizeof(futex_t), 0, futex_initializer, NULL);
    futex_mutex = mutex_create("futex");
    futex_map = hashmap_create_int("futex map", 10);
}