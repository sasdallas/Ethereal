/**
 * @file hexahedron/task/thread.c
 * @brief Main thread logic
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <kernel/task/process.h>
#include <kernel/mem/alloc.h>
#include <kernel/mem/mem.h>
#include <kernel/drivers/clock.h>
#include <kernel/debug.h>
#include <string.h>

/* Last TID */
unsigned long long last_tid = 1; // 0 is a kernel reserved TID

/* Log method */
#define LOG(status, ...) dprintf_module(status, "TASK:THREAD", __VA_ARGS__)

/**
 * @brief Create a new thread (internal)
 * @param parent The parent process of the thread
 * @param dir The directory of the thread
 * @param status The current status of the thread
 * @param flags The flags of the thread
 * 
 * @note No ticks are set and context will need to be saved
 */
static thread_t *thread_createStructure(process_t *parent, page_t *dir, int status,  int flags) {
    thread_t *thr = kmalloc(sizeof(thread_t));
    memset(thr, 0, sizeof(thread_t));
    thr->parent = parent;
    thr->status = status;
    thr->dir = dir;
    thr->flags = flags;
    thr->tid = last_tid++;

    // Thread ticks aren't updated because they should ONLY be updated when scheduler_insertThread is called
    return thr;
}

/**
 * @brief Create a new thread
 * @param parent The parent process of the thread
 * @param dir Directory to use (process' directory)
 * @param entrypoint The entrypoint of the thread (you can also set this later)
 * @param flags Flags of the thread
 * @returns New thread pointer, just save context & add to scheduler queue
 */
thread_t *thread_create(struct process *parent, page_t *dir, uintptr_t entrypoint, int flags) {
    // Create thread
    thread_t *thr = thread_createStructure(parent, dir, THREAD_STATUS_RUNNING, flags);

    // Switch directory to directory (as we will be mapping in it)
    page_t *prev_dir = current_cpu->current_dir;
    mem_switchDirectory(dir);

    // Allocate a kstack for the thread
    thr->kstack = mem_allocate(0, PROCESS_KSTACK_SIZE, MEM_ALLOC_HEAP, MEM_PAGE_KERNEL) + PROCESS_KSTACK_SIZE;

    if (!(flags & THREAD_FLAG_KERNEL) ) {
        // Allocate usermode stack 
        // !!!: This will need a lot of work done for when supporting multiple threads is ready
        // !!!: We need to have a system where multiple threads can share this memory region
        thr->stack = (MEM_USERMODE_STACK_REGION + MEM_USERMODE_STACK_SIZE);
        if (!(flags & THREAD_FLAG_CHILD)) {
            // !!!: Wow, this is bad. This is a hack for fork() support that prevents reallocating the child's stack as CoW is done on it
            // TODO: Probably just check if the region needs CoW?
            mem_allocate(thr->stack - THREAD_STACK_SIZE, THREAD_STACK_SIZE + PAGE_SIZE, MEM_DEFAULT, MEM_DEFAULT);
            vas_reserve(parent->vas, MEM_USERMODE_STACK_REGION + (MEM_USERMODE_STACK_SIZE-THREAD_STACK_SIZE), THREAD_STACK_SIZE, VAS_ALLOC_THREAD_STACK);
            memset((void*)(thr->stack - PAGE_SIZE), 0, PAGE_SIZE);
        }
    } else {
        // Don't bother, use the parent's kernel stack
        thr->stack = thr->kstack; 
    }

    // Initialize thread context
    arch_initialize_context(thr, entrypoint, thr->stack);

    // Switch back out of the directory back to previous directory
    mem_switchDirectory(prev_dir);

    return thr;
}

/**
 * @brief Destroys a thread. ONLY CALL ONCE THE THREAD IS FULLY READY TO BE DESTROYED
 * @param thr The thread to destroy
 */
int thread_destroy(thread_t *thr) {
    if (!thr) return 1;

    // TODO: Free memory

    // Free the thread's stack
    // mem_free(thr->kstack - PROCESS_KSTACK_SIZE, PROCESS_KSTACK_SIZE, MEM_ALLOC_HEAP);

    LOG(INFO, "Thread %p has exited successfully\n", thr);

    return 0;
}