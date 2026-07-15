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

#define FUTEX_LOCK() mutex_acquire(futex_mutex);
#define FUTEX_UNLOCK() mutex_release(futex_mutex);

/**
 * @brief Futex object initializer
 */
static int futex_initializer(slab_cache_t *cache, void *obj) {
    futex_t *f = obj;
    SPINLOCK_INIT(&f->lock);
    WAIT_QUEUE_INIT(&f->queue);
    f->waiters = 0;
    f->wakers = 0;
    return 0;
} 

/**
 * @brief Wait on a futex
 * @param pointer Pointer to the futex variable
 * @param value The value to look for
 * @param time Time to wait on
 */
int futex_wait(uint32_t *pointer, uint32_t val, const struct timespec *time) {
    // get the futex itself
    FUTEX_LOCK();
    uintptr_t phys = arch_mmu_physical(NULL, (uintptr_t)pointer);
    assert(phys);

    futex_t *ftx = NULL;
    if (!hashmap_has(futex_map, (void*)phys)) {
        ftx = slab_allocate(futex_cache);
        hashmap_set(futex_map, (void*)phys, ftx);
    } else {
        ftx = hashmap_get(futex_map, (void*)phys);
    }

    // this ordering should be fine
    spinlock_acquire(&ftx->lock);
    FUTEX_UNLOCK();

    int timeout = -1;
    if (time) {
        timeout = time->tv_sec * 1000 + time->tv_nsec / 1000000;
    }

    // we own the futex lock now, its time to sleepy
    int ret = 0;
    ftx->waiters++;
    for (;;) {
        wait_queue_node_t n;
        waitqueue_add(&ftx->queue, &n);

        if (__atomic_load_n(pointer, __ATOMIC_SEQ_CST) != val) {
            waitqueue_remove(&ftx->queue, &n);
            spinlock_release(&ftx->lock);
            return -EAGAIN;
        }

        spinlock_release(&ftx->lock);

        int w = waitqueue_wait(&ftx->queue, &n, timeout);

        spinlock_acquire(&ftx->lock);

        waitqueue_remove(&ftx->queue, &n);

        if (w != 0) {
            ftx->waiters--;
            ftx->wakers = min(ftx->waiters, ftx->wakers);
            ret = w;
            break;
        } else if (ftx->wakers != 0) {
            // we can leave now
            ftx->wakers--;
            ftx->waiters--;
            break;
        }
    }
    
    
    spinlock_release(&ftx->lock);
    
    // !!! sillyish
    FUTEX_LOCK();

    spinlock_acquire(&ftx->lock);
    if (ftx->waiters == 0) {
        hashmap_remove(futex_map, (void*)phys);
        FUTEX_UNLOCK();
        spinlock_release(&ftx->lock);

        slab_free(futex_cache, ftx);
    } else {
        spinlock_release(&ftx->lock);    
        FUTEX_UNLOCK();
    }
    
    return ret;
}

/**
 * @brief Wake up a futex
 * @param pointer Pointer to the futex
 * @param val How many waiters to wakeup
 */
int futex_wakeup(uint32_t *pointer, uint32_t val) {
    // get the futex itself
    FUTEX_LOCK();
    uintptr_t phys = arch_mmu_physical(NULL, (uintptr_t)pointer);
    assert(phys);

    futex_t *ftx = NULL;
    if (!hashmap_has(futex_map, (void*)phys)) {
        FUTEX_UNLOCK();
        return 0;
    } else {
        ftx = hashmap_get(futex_map, (void*)phys);
    }

    // this ordering should be fine
    spinlock_acquire(&ftx->lock);
    FUTEX_UNLOCK();
    
    int ret = ftx->waiters - ftx->wakers;
    assert(ret >= 0);
    if (ret > (int)val) {
        ret = (int)val;
    }

    ftx->wakers += ret;
    waitqueue_wakeup(&ftx->queue, ret);
    spinlock_release(&ftx->lock);

    return ret;
}

/**
 * @brief Initialize futexes
 */
void futex_init() {
    futex_cache = slab_createCache("futex cache", SLAB_CACHE_DEFAULT, sizeof(futex_t), 0, futex_initializer, NULL);
    futex_mutex = mutex_create("futex");
    futex_map = hashmap_create_int("futex map", 10);
}
