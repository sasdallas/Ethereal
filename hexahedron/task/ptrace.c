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
#include <kernel/task/syscall.h>
#include <kernel/debug.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <errno.h>

/* Log method */
#define LOG(status, ...) dprintf_module(status, "TASK:PTRACE", __VA_ARGS__)

/**
 * @brief Get a process' tracee by its PID
 * @param proc The process to search tracees of
 * @param pid The PID of the tracee
 */
process_t *ptrace_getTracee(process_t *proc, pid_t pid) {
    if (!proc->ptrace.tracees) return NULL;

    foreach(tracee_node, proc->ptrace.tracees) {
        if (((process_t*)tracee_node->value)->pid == pid) {
            return (process_t*)tracee_node->value;
        }
    }

    return NULL;
}

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
 * @brief Attach to a process (PTRACE_ATTACH)
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
 * @brief Trace the current process by its parent (PTRACE_TRACEME)
 */
int ptrace_traceme() {
    if (!current_cpu->current_process->parent) return -EINVAL;
    int r = ptrace_trace(current_cpu->current_process, current_cpu->current_process->parent);
    if (r) return r;

    signal_send(current_cpu->current_process, SIGSTOP);
    return 0;
}

/**
 * @brief Catch any system calls by the tracee (PTRACE_SYSCALL)
 * @param pid PID of the tracee
 * @param data Same as for @c ptrace_cont
 */
int ptrace_syscall(pid_t pid, void *data) {
    process_t *tracee = ptrace_getTracee(current_cpu->current_process, pid);
    if (!tracee) return -ESRCH;

    // Arrange for tracee to be stopped at next system call
    spinlock_acquire(&tracee->ptrace.lock);
    tracee->ptrace.events |= PROCESS_TRACE_SYSCALL;
    spinlock_release(&tracee->ptrace.lock);

    // Continue the tracee
    uintptr_t signum = data ? (uintptr_t)data : SIGCONT;
    signal_send(tracee, signum);
    LOG(DEBUG, "ptrace: tracing system calls\n");

    // Until later
    return 0;
}

/**
 * @brief Continue process (PTRACE_CONT)
 * @param pid The process to continue
 * @param data Optional data argument/signal to send
 */
int ptrace_cont(pid_t pid, void *data) {
    process_t *tracee = ptrace_getTracee(current_cpu->current_process, pid);
    if (!tracee) return -ESRCH;

    LOG(DEBUG, "Sending SIGCONT to tracee\n");
    uintptr_t signum = data ? (uintptr_t)data : SIGCONT;
    signal_send(tracee, signum);

    return 0;
}


/**
 * @brief Get the registers of a tracee
 * @param pid The PID of the tracee
 * @param data Pointer to regs structure
 */
int ptrace_getregs(pid_t pid, void *data) {
    SYSCALL_VALIDATE_PTR_SIZE(data, sizeof(struct user_regs_struct));
    process_t *tracee = ptrace_getTracee(current_cpu->current_process, pid);
    if (!tracee) return -ESRCH;

    // Get user registers
    arch_to_user_regs((struct user_regs_struct*)data, tracee->main_thread);
    return 0;
}

/**
 * @brief Set the registers of a tracee
 * @param pid The PID of the tracee
 * @param data Pointer to regs structure
 */
int ptrace_setregs(pid_t pid, void *data) {
    SYSCALL_VALIDATE_PTR_SIZE(data, sizeof(struct user_regs_struct));
    process_t *tracee = ptrace_getTracee(current_cpu->current_process, pid);
    if (!tracee) return -ESRCH;

    // Set user registers
    arch_from_user_regs((struct user_regs_struct*)data, tracee->main_thread);
    return 0;
}

/**
 * @brief Alert that an event has been completed
 * @param event The event that was completed
 */
int ptrace_event(int event) {
    // TODO: For ptrace events, how are we supposed to keep track of each thread?
    process_t *process = current_cpu->current_process;
    if (!process->ptrace.tracer || !(process->ptrace.events & event)) return 0; // Not listening for or not being traced

    // Signal to the process, turn off said event
    spinlock_acquire(&process->ptrace.lock);
    process->ptrace.events &= ~(event);
    spinlock_release(&process->ptrace.lock);

    // We are the tracee, so let's put ourselves together
    __sync_or_and_fetch(&process->flags, PROCESS_SUSPENDED);

    // Wakeup our tracer?
    process->exit_reason = PROCESS_EXIT_SIGNAL;
    process->exit_status = SIGTRAP;

    process_t *tracer = process->ptrace.tracer;
    if (tracer->waitpid_queue && tracer->waitpid_queue->length) {
        // TODO: Locking?
        foreach(thr_node, tracer->waitpid_queue) {
            thread_t *thr = (thread_t*)thr_node->value;
            sleep_wakeup(thr);
        }
    }

    // Night night
    process_yield(0);

    process->exit_reason = 0;
    process->exit_status = 0;

    return 0;
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
        case PTRACE_SYSCALL:
            return ptrace_syscall(pid, data);
        case PTRACE_CONT:
            return ptrace_cont(pid, data);
        case PTRACE_GETREGS:
            return ptrace_getregs(pid, data);
        case PTRACE_SETREGS:
            return ptrace_setregs(pid, data);
        default:
            LOG(WARN, "Unknown or unimplemented ptrace operation %d\n", op);
            return -ENOSYS;
    }

    return 0;
}