/**
 * @file hexahedron/task/ptrace.c
 * @brief Process ptrace system
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
#include <kernel/debug.h>
#include <sys/ptrace.h>
#include <errno.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "TASK:PTRACE", __VA_ARGS__)

/**
 * @brief Trace @c tracee by @c tracer
 * @param tracee The process being traced
 * @param tracer The process tracing
 */
int ptrace_trace(process_t *tracee, process_t *tracer) {
    LOG(DEBUG, "Process %s:%d is being traced by %s:%d\n", tracee->name, tracee->pid, tracer->name, tracer->pid);

    spinlock_acquire(&tracee->ptrace.lock);
    spinlock_acquire(&tracer->ptrace.lock);

    if (tracee->ptrace.tracer) {
        spinlock_release(&tracee->ptrace.lock);
        spinlock_release(&tracer->ptrace.lock);
        return -EPERM;
    }

    // Set the tracer
    tracee->ptrace.tracer = tracer;

    if (!tracer->ptrace.tracees) {
        tracer->ptrace.tracees = list_create("tracees");
    }

    // Add to tracee list
    list_append(tracer->ptrace.tracees, tracee);

    spinlock_release(&tracee->ptrace.lock);
    spinlock_release(&tracer->ptrace.lock);
    
    return 0;
}

/**
 * @brief Attach to a process
 * @param pid Tracee
 */
int ptrace_attach(pid_t pid) {
    process_t *tracee = process_getFromPID(pid);
    if (!tracee) return -ESRCH;

    // The process must be the same user or be root to trace
    if (!PROC_IS_ROOT(current_cpu->current_process) && tracee->euid != current_cpu->current_process->euid) return -EPERM;

    // The tracee must not already be being traced
    if (tracee->ptrace.tracer) return -EPERM;

    int r = ptrace_trace(tracee, current_cpu->current_process);
    if (r) return r;

    signal_send(tracee, SIGSTOP);
    return 0;
}

/**
 * @brief Trace the current process by its parent
 */
int ptrace_traceme() {
    if (!current_cpu->current_process->parent) return -EINVAL;
    return ptrace_trace(current_cpu->current_process, current_cpu->current_process->parent);
}

/**
 * @brief Handle a ptrace request by the current process
 * @param op The operation to perform
 * @param pid The target PID
 * @param addr Address
 * @param data Data
 */
long ptrace_handle(enum __ptrace_request op, pid_t pid, void *addr, void *data) {
    switch (op) {
        case PTRACE_TRACEME:
            return ptrace_traceme();
        case PTRACE_ATTACH:
            return ptrace_attach(pid);
        default:
            LOG(WARN, "Unknown or unimplemented ptrace operation %d\n", op);
            return -ENOSYS;
    }

    return 0;
}