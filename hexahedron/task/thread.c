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
 * @param ctx The context of the thread
 * @param status The current status of the thread
 * @param flags The flags of the thread
 * 
 * @note No ticks are set and context will need to be saved
 */
static thread_t *thread_createStructure(process_t *parent, vmm_context_t *ctx, int status,  int flags) {
    thread_t *thr = kmalloc(sizeof(thread_t));
    memset(thr, 0, sizeof(thread_t));
    thr->parent = parent;
    thr->status = status;
    thr->ctx = ctx ? ctx : vmm_kernel_context;
    thr->flags = flags;
    thr->tid = last_tid++;
    thr->sched_node.value = thr;

    // Thread ticks aren't updated because they should ONLY be updated when scheduler_insertThread is called
    return thr;
}

/**
 * @brief Create a new thread
 * @param parent The parent process of the thread
 * @param ctx Context to use
 * @param entrypoint The entrypoint of the thread (you can also set this later)
 * @param flags Flags of the thread
 * @returns New thread pointer, just save context & add to scheduler queue
 */
thread_t *thread_create(struct process *parent, vmm_context_t *ctx, uintptr_t entrypoint, int flags) {
    // Create thread
    thread_t *thr = thread_createStructure(parent, ctx, THREAD_STATUS_RUNNING, flags);

    if (!ctx) ctx = parent->ctx;

    // Switch into the context
    vmm_context_t *prev_ctx = current_cpu->current_context;
    vmm_switch(ctx);

    // Allocate a kstack for the thread
    thr->kstack = (uintptr_t)kzalloc(PROCESS_KSTACK_SIZE) + PROCESS_KSTACK_SIZE;

    if (!(flags & THREAD_FLAG_KERNEL) ) {
        // Allocate user mode stack 
        thr->stack = (MEM_USERMODE_STACK_REGION + MEM_USERMODE_STACK_SIZE);
        if (!(flags & THREAD_FLAG_CHILD)) {
            vmm_map((void*)(thr->stack - THREAD_STACK_SIZE), THREAD_STACK_SIZE, VM_FLAG_ALLOC | VM_FLAG_FIXED, MMU_FLAG_RW | MMU_FLAG_USER | MMU_FLAG_PRESENT);
            memset((void*)(thr->stack - PAGE_SIZE), 0, PAGE_SIZE);
        }
    } else {
        // Don't bother, use the parent's kernel stack
        thr->stack = thr->kstack; 
    }

    // Initialize thread context
    arch_initialize_context(thr, entrypoint, thr->stack);

    // Switch back
    vmm_switch(prev_ctx);

    return thr;
}

/**
 * @brief Destroys a thread. ONLY CALL ONCE THE THREAD IS FULLY READY TO BE DESTROYED
 * @param thr The thread to destroy
 */
int thread_destroy(thread_t *thr) {
    if (!thr) return 1;

    // TODO: Free memory
    __sync_or_and_fetch(&thr->status, THREAD_STATUS_STOPPED);

    // Free the thread's stack
    if (thr->kstack) kfree((void*)(thr->kstack - PROCESS_KSTACK_SIZE));

    kfree(thr);

    LOG(DEBUG, "******************************************** Thread %p destroyed, kstack %p\n", thr, thr->kstack);

    return 0;
}