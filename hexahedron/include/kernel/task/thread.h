/**
 * @file hexahedron/include/kernel/task/thread.h
 * @brief Thread file
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_TASK_THREAD_H
#define KERNEL_TASK_THREAD_H

/**** INCLUDES ****/
#include <stdint.h>
#include <kernel/arch/arch.h>
#include <kernel/mm/vmm.h>
#include <kernel/task/sleep.h>
#include <kernel/task/signal.h>
#include <kernel/subsystems/timemonitor.h>
#include <kernel/misc/procmask.h>
#include <sys/signal.h>
#include <sys/types.h>

/**** DEFINITIONS ****/

// Thread status flags
#define THREAD_STATUS_STOPPED       0x02
#define THREAD_STATUS_RUNNING       0x04
#define THREAD_STATUS_SLEEPING      0x08
#define THREAD_STATUS_STOPPING      0x10

// Thread flags
#define THREAD_FLAG_DEFAULT         0x00
#define THREAD_FLAG_KERNEL          0x01
#define THREAD_FLAG_NO_PREEMPT      0x02
#define THREAD_FLAG_CHILD           0x04    // Thread is a child. NOT PRESERVED. Tells thread_create() not to allocate a stack and mess up potential CoW
#define THREAD_FLAG_NEEDS_RESCHED   0x08    // Set by the scheduler callback, triggers reschedule on irq_handler
#define THREAD_FLAG_IDLE            0x10    // This is the idle thread

// Stack size of thread
#define THREAD_STACK_SIZE           PAGE_SIZE * 16

/**** TYPES ****/

// Prototype
struct process;
struct syscall;

/**
 * @brief Thread structure. Contains an execution path in a process.
 */
typedef struct thread {
    // GENERAL VARIABLES
    struct thread *next;                    // Next thread, only used when this is not the main thread.
    struct process *parent;
    unsigned int status;
    unsigned int flags;

    // BLOCKING VARIABLES
    thread_sleep_t sleep;                   // Sleep structure

    // THREAD VARIABLES
    arch_context_t context;                 // Thread context (defined by architecture)
    uint8_t fp_regs[512] __attribute__((aligned(16))); // FPU registers (TEMPORARY - should be moved into arch_context?)

    // SIGNALS
    spinlock_t siglock;
    proc_signal_t signals[_NSIG];
    sigset_t pending_signals;
    sigset_t blocked_signals;
    sigset_t forced_signals;

    // SCHEDULER
    void *sched;
    int nice;
    procmask_t affinity;

    // OTHER
    vmm_context_t *ctx;                     // Context
    struct _registers *regs;                // Registers of the thread
    uintptr_t stack;                        // Thread stack (kernel will load kstack in TSS)
    uintptr_t kstack;                       // Kernel stack
    struct syscall *syscall;                // The current system call of the thread
    thread_times_t times;                   // Thread times

    // PTHREAD RELATED
    pid_t tid;                              // Thread ID
} thread_t;

/* ASM constraints */
#ifdef __ARCH_X86_64__
MUST_BE_AT_OFFSET(thread_t, sleep.lock.lock, 0x3C);
MUST_BE_AT_OFFSET(thread_t, status, 0x10);
MUST_BE_AT_OFFSET(thread_t, context, 0x70);
#endif

/**** MACROS ****/

/* Push something onto a thread's stack */
#define THREAD_PUSH_STACK(stack, type, value) stack -= sizeof(type); \
                                                *((volatile type*)stack) = (type)value

/* Push something onto a thread's stack (size) */
#define THREAD_PUSH_STACK_SIZE(stack, size, value) { for (size_t i = 0; i < size; i++) { THREAD_PUSH_STACK(stack, uint8_t, value[i]); }; }

/* Push a string (added because strings must be pushed in reverse) */
#define THREAD_PUSH_STACK_STRING(stack, length, string) { for (ssize_t i = length; i >= 0; i--) { THREAD_PUSH_STACK(stack, uint8_t, string[i]); }}

/**** FUNCTIONS ****/

/**
 * @brief Create a new thread
 * @param parent The parent process of the thread
 * @param ctx Context to use
 * @param entrypoint The entrypoint of the thread (you can also set this later)
 * @param flags Flags of the thread
 * @returns New thread pointer, just save context & add to scheduler queue
 */
thread_t *thread_create(struct process *parent, vmm_context_t *ctx, uintptr_t entrypoint, int flags);

/**
 * @brief Destroys a thread. ONLY CALL ONCE THE THREAD IS FULLY READY TO BE DESTROYED
 * @param thr The thread to destroy
 */
int thread_destroy(thread_t *thr);

/**
 * @brief Exit from the current thread, non-returning
 */
void thread_exit();

#endif