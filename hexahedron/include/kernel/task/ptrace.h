/**
 * @file hexahedron/include/kernel/task/ptrace.h
 * @brief Kernel ptrace system
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef KERNEL_TASK_PTRACE_H
#define KERNEL_TASK_PTRACE_H

/**** INCLUDES ****/
#include <sys/ptrace.h>
#include <structs/list.h>
#include <kernel/misc/spinlock.h>

/**** TYPES ****/

typedef struct process_ptrace {
    spinlock_t lock;                    // ptrace lock
    struct process *proc;               // Process of the trace
    list_t *tracees;                    // Processes being traced
    struct process *tracer;             // The current tracer of the process
    int events;                         // Events to handle for the process (PROCESS_TRACE_xxx)
} process_ptrace_t;

/**** FUNCTIONS ****/


/**
 * @brief Handle a ptrace request by the current process
 * @param op The operation to perform
 * @param pid The target PID
 * @param addr Address
 * @param data Data
 */
long ptrace_handle(enum __ptrace_request op, pid_t pid, void *addr, void *data);

#endif