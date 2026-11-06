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
 */

#include <kernel/task/process.h>
#include <kernel/misc/mutex.h>
#include <kernel/debug.h>
#include <kernel/mm/alloc.h>
#include <kernel/mm/vmm.h>
#include <kernel/misc/util.h>
#include <kernel/panic.h>
#include <kernel/event.h>
#include <errno.h>
#include <assert.h>
#include <string.h>

#define LOG(status, ...) dprintf_module(status, "TASK:FUTEX", __VA_ARGS__)

slab_cache_t *futex_cache = NULL;
mutex_t *futex_mutex = NULL;

/* futex_entry is because of CoW pages */
/* CoW can change the physical address of the page which previously the futexes keyed on */
/* ideally, there would be an interface to lock/ensure a PMM page is not CoW + cannot be CoWed */
/* and yet there isn't so we key by address/space */
typedef struct futex_entry {
    struct futex_entry *next;
    futex_t futex;
    vmm_space_t *space;
    uintptr_t address;
} futex_entry_t;

static futex_entry_t *futex_entries = NULL;

#define FUTEX_LOCK() mutex_acquire(futex_mutex);
#define FUTEX_UNLOCK() mutex_release(futex_mutex);

/**
 * @brief Futex object initializer
 */
static int futex_initializer(slab_cache_t *cache, void *obj) {
    futex_entry_t *entry = obj;
    futex_t *f = &entry->futex;
    MUTEX_INIT(&f->lock);
    WAIT_QUEUE_INIT(&f->queue);
    f->waiters = 0;
    f->wakers = 0;
    f->references = 0;
    return 0;
}

/**
 * @brief Make futex key
 */
static int futex_make_key(uint32_t *pointer, futex_entry_t *key) {
    vmm_space_t *space = vmm_getSpaceForAddress(pointer);
    if (!space) return -EFAULT;

    mutex_acquire(space->mut);
    vmm_memory_range_t *range = vmm_getRange(space, (uintptr_t)pointer, sizeof(*pointer));
    if (!range) {
        mutex_release(space->mut);
        return -EFAULT;
    }
    mutex_release(space->mut);

    key->space = (range->vmm_flags & VM_FLAG_SHARED) ? NULL : space;
    key->address = (range->vmm_flags & VM_FLAG_SHARED) ? arch_mmu_physical(NULL, (uintptr_t)pointer) : (uintptr_t)pointer;
    return key->address ? 0 : -EFAULT;
}

/**
 * @brief Find futex by key
 */
static futex_entry_t *futex_find(futex_entry_t *key) {
    for (futex_entry_t *entry = futex_entries; entry; entry = entry->next) {
        if (entry->space == key->space && entry->address == key->address) return entry;
    }
    return NULL;
}

/**
 * @brief Release a futex
 */
static void futex_release(futex_entry_t *entry) {
    FUTEX_LOCK();
    futex_t *ftx = &entry->futex;
    assert(ftx->references > 0);

    ftx->references--;
    if (ftx->references == 0) {
        assert(ftx->waiters == 0);
        futex_entry_t **link = &futex_entries;
        while (*link && *link != entry) link = &(*link)->next;
        assert(*link);
        *link = entry->next;
        FUTEX_UNLOCK();
        slab_free(futex_cache, entry);
        return;
    }

    FUTEX_UNLOCK();
}

/**
 * @brief Wait on a futex
 * @param pointer Pointer to the futex variable
 * @param value The value to look for
 * @param time Time to wait on
 */
int futex_wait(uint32_t *pointer, uint32_t val, const struct timespec *time) {
    futex_entry_t key;
    int ret = futex_make_key(pointer, &key);
    if (ret) return ret;

    FUTEX_LOCK();

    // create the futex maybe
    futex_entry_t *entry = futex_find(&key);
    if (!entry) {
        entry = slab_allocate(futex_cache);
        entry->space = key.space;
        entry->address = key.address;
        entry->next = futex_entries;
        futex_entries = entry;
    }

    futex_t *ftx = &entry->futex;
    ftx->references++;

    // this ordering should be fine
    mutex_acquire(&ftx->lock);
    FUTEX_UNLOCK();

    int timeout = -1;
    if (time) {
        timeout = time->tv_sec * 1000 + (time->tv_nsec + 999999) / 1000000;
    }

    // we own the futex lock now, its time to sleepy
    ftx->waiters++;
    for (;;) {
        wait_queue_node_t n;
        waitqueue_add(&ftx->queue, &n);

        if (__atomic_load_n(pointer, __ATOMIC_SEQ_CST) != val) {
            waitqueue_remove(&ftx->queue, &n);
            ftx->waiters--;
            ret = -EAGAIN;
            break;
        }

        mutex_release(&ftx->lock);
        int w = waitqueue_wait(&ftx->queue, &n, timeout);
        mutex_acquire(&ftx->lock);

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

    mutex_release(&ftx->lock);

    futex_release(entry);
    return ret;
}

/**
 * @brief Wake up a futex
 * @param pointer Pointer to the futex
 * @param val How many waiters to wakeup
 */
int futex_wakeup(uint32_t *pointer, uint32_t val) {
    futex_entry_t key;
    int ret = futex_make_key(pointer, &key);
    if (ret) return ret;

    FUTEX_LOCK();
    futex_entry_t *entry = futex_find(&key);
    if (!entry) {
        FUTEX_UNLOCK();
        return 0;
    }
    futex_t *ftx = &entry->futex;
    ftx->references++;

    // this ordering should be fine
    mutex_acquire(&ftx->lock);
    FUTEX_UNLOCK();

    ret = ftx->waiters - ftx->wakers;
    assert(ret >= 0);
    if (ret > (int)val) {
        ret = (int)val;
    }

    ftx->wakers += ret;
    if (ret) waitqueue_wakeup(&ftx->queue, ret);
    mutex_release(&ftx->lock);

    futex_release(entry);
    return ret;
}

/**
 * @brief Initialize futexes
 */
void futex_init() {
    futex_cache = slab_createCache("futex cache", SLAB_CACHE_DEFAULT, sizeof(futex_entry_t), 0, futex_initializer, NULL);
    futex_mutex = mutex_create("futex");
}
