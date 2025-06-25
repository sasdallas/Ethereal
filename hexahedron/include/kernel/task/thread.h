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
#include <kernel/mem/mem.h>
#include <kernel/task/sleep.h>
#include <sys/types.h>

/**** DEFINITIONS ****/

// Thread status flags
#define THREAD_STATUS_KERNEL        0x01
#define THREAD_STATUS_STOPPED       0x02
#define THREAD_STATUS_RUNNING       0x04
#define THREAD_STATUS_SLEEPING      0x08
#define THREAD_STATUS_STOPPING      0x10

// Thread flags
#define THREAD_FLAG_DEFAULT         0x00
#define THREAD_FLAG_KERNEL          0x01
#define THREAD_FLAG_NO_PREEMPT      0x02    // This only works on threads with THREAD_FLAG_KERNEL
#define THREAD_FLAG_CHILD           0x04    // Thread is a child. NOT PRESERVED. Tells thread_create() not to allocate a stack and mess up potential CoW

// Stack size of thread
#define THREAD_STACK_SIZE           PAGE_SIZE * 16

/**** TYPES ****/

// Prototype
struct process;

/**
 * @brief Thread structure. Contains an execution path in a process.
 */
typedef struct thread {
    // GENERAL VARIABLES
    struct process *parent;     // Parent process
    unsigned int status;        // Status of this thread
    unsigned int flags;         // Flags of the thread

    // SCHEDULER VARIABLES
    node_t *sched_node;         // Scheduler node
    time_t preempt_ticks;       // Ticks until the thread is preempted
    time_t total_ticks;         // Total amount of ticks the thread has been running for
    time_t start_ticks;         // Starting ticks

    // BLOCKING VARIABLES
    thread_sleep_t *sleep;      // Sleep structure

    // THREAD VARIABLES
    arch_context_t context;     // Thread context (defined by architecture)
    uint8_t fp_regs[512] __attribute__((aligned(16))); // FPU registers (TEMPORARY - should be moved into arch_context?)

    // OTHER
    page_t *dir;                // Page directory for the thread
    uintptr_t stack;            // Thread stack (kernel will load kstack in TSS)
    uintptr_t kstack;           // Kernel stack
    
    // PTHREAD RELATED
    pid_t tid;                  // Thread ID
    list_t *joiners;            // List of joiners
    void *retval;               // Return value of the thread
                                // NOTE: This is a weird solution since you can keep joining the same thread. We will destroy everything we can in this thread object
                                // NOTE: except the actual object itself, until the process exits.
    spinlock_t joiner_lck;      // Joiner lock
} thread_t;


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
 * @param dir Directory to use (process' directory)
 * @param entrypoint The entrypoint of the thread (you can also set this later)
 * @param flags Flags of the thread
 * @returns New thread pointer, just save context & add to scheduler queue
 */
thread_t *thread_create(struct process *parent, page_t *dir, uintptr_t entrypoint, int flags);

/**
 * @brief Destroys a thread. ONLY CALL ONCE THE THREAD IS FULLY READY TO BE DESTROYED
 * @param thr The thread to destroy
 */
int thread_destroy(thread_t *thr);

#endif