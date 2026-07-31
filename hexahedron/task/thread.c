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
#include <kernel/mm/alloc.h>
#include <kernel/mm/vmm.h>
#include <kernel/drivers/clock.h>
#include <kernel/misc/util.h>
#include <kernel/debug.h>
#include <kernel/smp.h>
#include <string.h>

/* Last TID */
unsigned long long last_tid = 1; // 0 is a kernel reserved TID

slab_cache_t *thread_cache = NULL;

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
    if (!thread_cache) thread_cache = slab_createCache("thread cache", SLAB_CACHE_DEFAULT, sizeof(thread_t), 0, NULL, NULL);

    thread_t *thr = slab_allocate(thread_cache);
    memset(thr, 0, sizeof(thread_t));
    thr->parent = parent;
    thr->status = status;
    thr->ctx = ctx ? ctx : vmm_kernel_context;
    thr->flags = flags;
    thr->tid = __atomic_add_fetch(&last_tid, 1, __ATOMIC_RELAXED);
    
    thr->affinity = smp_online_cpus;
    thr->nice = 0;
    sched_initThread(thr);
    
    return thr;
}

/**
 * @brief Thread entrypoint
 * Due to the handoff method in @c arch_switch_context this is required. New threads will jump to @c arch_thread_entry
 * and that will then jump over here.
 */
__attribute__((no_caller_saved_registers)) void thread_entrypoint(thread_t *previous, void (*entrypoint)()) {
    if (previous != NULL) {
        sched_insert(previous);
    }

    // Begin new threads with interrupts enabled
    hal_setInterruptState(HAL_INTERRUPTS_ENABLED);
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

    if (!(flags & THREAD_FLAG_KERNEL)) {
        // Allocate user mode stack 
        thr->stack = (MMU_USERMODE_STACK_REGION + MMU_USERMODE_STACK_SIZE);
        if (!(flags & THREAD_FLAG_CHILD)) {
            vmm_map((void*)(thr->stack - THREAD_STACK_SIZE), THREAD_STACK_SIZE, VM_FLAG_ALLOC | VM_FLAG_FIXED, MMU_FLAG_WRITE | MMU_FLAG_USER | MMU_FLAG_PRESENT);
            memset((void*)(thr->stack - PAGE_SIZE), 0, PAGE_SIZE);
            thr->ctx->space->metrics.stack += THREAD_STACK_SIZE; 
        }
    } else {
        // Don't bother, use the parent's kernel stack
        thr->stack = thr->kstack;
    }

    // Push the entrypoint to the stack
    THREAD_PUSH_STACK(thr->stack, uintptr_t, entrypoint);

    // For threads that aren't kernel threads a context must be pushed
    if ((thr->flags & THREAD_FLAG_KERNEL) == 0) {
        THREAD_PUSH_STACK(thr->stack, uintptr_t, 0x0);
    }

    // Initialize the handler context
    arch_initialize_context(thr, (uintptr_t)&arch_thread_entry, thr->stack);

    // Switch back
    vmm_switch(prev_ctx);

    return thr;
}

/**
 * @brief Destroys a thread. ONLY CALL ONCE THE THREAD IS FULLY READY TO BE DESTROYED
 * @param thr The thread to destroy
 */
int thread_destroy(thread_t *thr) {
    // Free the thread's stack
    LOG(DEBUG, "******************************************** Thread %p destroying\n", thr);
    if (thr->kstack) kfree((void*)(thr->kstack - PROCESS_KSTACK_SIZE));

    sched_freeThread(thr);
    slab_free(thread_cache, thr);

    return 0;
}


/**
 * @brief Safe internal-exit
 */
__attribute__((no_caller_saved_registers)) void thread_safeExit(thread_t *arg) {
    thread_t *t = (thread_t*)arg;
    __sync_or_and_fetch(&t->status, THREAD_STATUS_STOPPED);
    __sync_and_and_fetch(&t->status, ~(THREAD_STATUS_STOPPING));
    __atomic_fetch_sub(&t->parent->nthreads, 1, __ATOMIC_SEQ_CST);

    if (t->parent->nthreads == 0) {
        reaper_push(t->parent);
    }

    process_switchNextThread();
}

/**
 * @brief Exit from the current thread, non-returning
 */
void thread_exit() {
    thread_t *t = current_cpu->current_thread;
    timemonitor_updateThreadExit();

    // process_yield() will kill us
    LOG(DEBUG, "thread_exit %p\n", t);

    // IRQs should be disabled from now on to prevent preemption
    hal_setInterruptState(HAL_INTERRUPTS_DISABLED);

    // Notify scheduler that we are exiting
    sched_event(t, SCHED_EVENT_EXIT);

    // !!! TODO Replace this stupid design with something better.
    // !!! The idea of using a func is fine but using the idle process is silly
    arch_handle_threadexit(current_cpu->idle_process->main_thread, t);
}
