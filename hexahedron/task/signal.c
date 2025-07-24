/**
 * @file hexahedron/task/signal.c
 * @brief Signal handler for tasks
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
#include <kernel/panic.h>
#include <errno.h>
#include <string.h>

/* Default action list */
const sa_handler signal_default_action[] = {
    [SIGABRT]           = SIGNAL_ACTION_TERMINATE_CORE,
    [SIGALRM]           = SIGNAL_ACTION_TERMINATE,
    [SIGBUS]            = SIGNAL_ACTION_TERMINATE_CORE,
    [SIGCHLD]           = SIGNAL_ACTION_IGNORE,
    [SIGCONT]           = SIGNAL_ACTION_CONTINUE,
    [SIGFPE]            = SIGNAL_ACTION_TERMINATE_CORE,
    [SIGHUP]            = SIGNAL_ACTION_TERMINATE,
    [SIGILL]            = SIGNAL_ACTION_TERMINATE_CORE,
    [SIGINT]            = SIGNAL_ACTION_TERMINATE,
    [SIGKILL]           = SIGNAL_ACTION_TERMINATE,      // NOTE: Cannot be ignored
    [SIGPIPE]           = SIGNAL_ACTION_TERMINATE,
    [SIGQUIT]           = SIGNAL_ACTION_TERMINATE_CORE,
    [SIGSEGV]           = SIGNAL_ACTION_TERMINATE_CORE,
    [SIGSTOP]           = SIGNAL_ACTION_STOP,
    [SIGTERM]           = SIGNAL_ACTION_TERMINATE,
    [SIGTSTP]           = SIGNAL_ACTION_STOP,
    [SIGTTIN]           = SIGNAL_ACTION_STOP,
    [SIGTTOU]           = SIGNAL_ACTION_STOP,
    [SIGUSR1]           = SIGNAL_ACTION_TERMINATE,
    [SIGUSR2]           = SIGNAL_ACTION_TERMINATE,
    [SIGPOLL]           = SIGNAL_ACTION_TERMINATE,
    [SIGPROF]           = SIGNAL_ACTION_TERMINATE,
    [SIGSYS]            = SIGNAL_ACTION_TERMINATE_CORE,
    [SIGTRAP]           = SIGNAL_ACTION_TERMINATE_CORE,
    [SIGURG]            = SIGNAL_ACTION_IGNORE,
    [SIGVTALRM]         = SIGNAL_ACTION_TERMINATE,
    [SIGXCPU]           = SIGNAL_ACTION_TERMINATE_CORE,
    [SIGXFSZ]           = SIGNAL_ACTION_TERMINATE_CORE,
    [SIGWINCH]          = SIGNAL_ACTION_IGNORE,
    [SIGCANCEL]         = SIGNAL_ACTION_IGNORE,
    [SIGDISPLAY]        = SIGNAL_ACTION_IGNORE,
};

/* Pending signal set */
#define SIGBIT(signum) (1 << signum)
#define SIGNAL_MARK_PENDING(thr, signum) (thr->pending_signals) |= SIGBIT(signum)
#define SIGNAL_UNMARK_PENDING(thr, signum) (thr->pending_signals) &= ~(SIGBIT(signum))
#define SIGNAL_IS_BLOCKED(thr, signum) (thr->blocked_signals & SIGBIT(signum) && !(signum == SIGKILL) && !(signum == SIGSTOP))
#define SIGNAL_IS_PENDING(thr, signum) (thr->pending_signals & SIGBIT(signum) && !SIGNAL_IS_BLOCKED(thr, signum))
#define SIGNAL_ANY_PENDING(thr) (thr->pending_signals & ~thr->blocked_signals)

/* Log method */
#define LOG(status, ...) dprintf_module(status, "TASK:SIGNAL", __VA_ARGS__)

/**
 * @brief Send a signal to a specific thread
 * @param thread The thread to send the signal to
 * @param signal The signal to send to the thread
 * @returns 0 on success
 */
int signal_sendThread(struct thread *thr, int signal) {
    if (signal < 0 || signal >= NUMSIGNALS) return -EINVAL;

    // Are they trying to continue a process?
    if (THREAD_SIGNAL(thr, signal).handler == SIGNAL_ACTION_CONTINUE && thr->status & THREAD_STATUS_SLEEPING) {
        // TODO: Continue
        LOG(ERR, "Cannot continue a process as this is unimplemented\n");
        return -ENOTSUP;
    }

    // Is this signal blocked?
    if (SIGNAL_IS_BLOCKED(thr, signal)) {
        return 0;
    }

    // Do we just ignore it? Don't waste time on that
    if (THREAD_SIGNAL(thr, signal).handler == SIGNAL_ACTION_IGNORE) {
        return 0;
    }

    // Mark the signal as pending
    spinlock_acquire(&thr->siglock);
    SIGNAL_MARK_PENDING(thr, signal);
    spinlock_release(&thr->siglock);

    // TODO: Interrupt system calls

    // Syncronously execute sleep callback, because why not
    extern void sleep_callback(uint64_t ticks);
    sleep_callback(0);

    // Wake them up if they aren't us
    if (thr != current_cpu->current_thread && (thr->parent->flags & PROCESS_SUSPENDED)) {
        // Wakeup bro
        // TODO: Use threads, not processes...
        LOG(DEBUG, "Waking up thread\n");
        thr->parent->flags &= ~(PROCESS_SUSPENDED);
        scheduler_insertThread(thr);
    }

    return 0;
}

/**
 * @brief Send a signal to a process
 * @param proc The process to send the signal to
 * @param signal The signal to send to the process
 * @returns 0 on success, otherwise error code
 */
int signal_send(struct process *proc, int signal) {
    LOG(DEBUG, "Sending signal %d to process pid %d\n", signal, proc->pid);
    if (signal < 0 || signal >= NUMSIGNALS) return -EINVAL;

    return signal_sendThread(proc->main_thread, signal);
}

/**
 * @brief Internal method to handle signal
 * @returns 1 if the signal was handled and to return, 0 if not handled
 */
static int signal_try_handle(thread_t *thr, int signum, registers_t *regs) {
    process_t *proc = thr->parent;
    if (!proc) return 0;

    // Get signal and handler
    proc_signal_t *sig = &THREAD_SIGNAL(thr, signum);
    sa_handler handler = (sig->handler ? sig->handler : signal_default_action[signum]);

    // Reset?
    if (sig->flags & SA_RESETHAND) {
        sig->handler = SIGNAL_ACTION_DEFAULT;
    }

    // We're gonna handle this signal so unset it
    SIGNAL_UNMARK_PENDING(thr, signum);

    // Handle appropriately
    if (handler == SIGNAL_ACTION_DEFAULT) {
        kernel_panic_extended(UNKNOWN_CORRUPTION_DETECTED, "signal", "*** The default signal handler says to call the default signal handler.\n");
    } else if (handler == SIGNAL_ACTION_CONTINUE) {
        // We just got a SIGCONT with no handler (SIGCONT already woke us up)
        return 0;
    } else if (handler == SIGNAL_ACTION_STOP) {
        // Set up our waitpid parameters
        proc->exit_status = signum;
        proc->exit_reason = PROCESS_EXIT_SIGNAL;

        // Stop process
        proc->flags &= ~(PROCESS_RUNNING);
        proc->flags |= PROCESS_SUSPENDED;

        if (proc->thread_list && proc->thread_list->length) {
            LOG(ERR, "SIGNAL_ACTION_STOP with multiple threads is not implemented\n");
        }

        // Wakeup any parents that are waiting
        // TODO: Send SIGCHLD and put our other threads to sleep
        if (proc->parent) {
            if (proc->parent->waitpid_queue && proc->parent->waitpid_queue->length) {
                // TODO: Locking?
                foreach(thr_node, proc->parent->waitpid_queue) {
                    thread_t *thr = (thread_t*)thr_node->value;
                    sleep_wakeup(thr);
                }
            }
        }

        // Suspend ourselves
        LOG(DEBUG, "Suspending process - received SIGNAL_ACTION_STOP\n");

        // Now enter a loop forever with no escape!!
        spinlock_release(&thr->siglock);
        do {
            process_yield(0);
        } while (!SIGNAL_ANY_PENDING(thr));
        spinlock_acquire(&thr->siglock);

        // Oh, we escaped. Go back to normal exit state.
        proc->exit_status = PROCESS_EXIT_NORMAL;

        // Done, go back and handle another
        return 0;
    } else if (handler == SIGNAL_ACTION_IGNORE) {
        // Ignore signal
        return 0;
    } else if (handler == SIGNAL_ACTION_TERMINATE || handler == SIGNAL_ACTION_TERMINATE_CORE) {
        // Terminate the process
        proc->exit_status = PROCESS_EXIT_SIGNAL;
        process_exit(proc, signum);
        return 2;
    }

    // Else, let's have it call the handler.
    LOG(DEBUG, "Handling signal %d for process PID %d thread TID %d (handler: %p)\n", signum, proc->pid, thr->tid, handler);

    // If the process does not have a userspace allocation, create one.
    // !!!: This probably needs to be refactored?
    extern uintptr_t __userspace_start, __userspace_end;
    if (!proc->userspace) {
        proc->userspace = vas_allocate(proc->vas, PAGE_SIZE);
        if (!proc->userspace) {
            kernel_panic_extended(OUT_OF_MEMORY, "signal", "*** Out of memory when allocating a signal trampoline.\n");
        }

        proc->userspace->type = VAS_ALLOC_SIGNAL_TRAMP;

        // Copy in the userspace section
        memcpy((void*)proc->userspace->base, &__userspace_start, (uintptr_t)&__userspace_end - (uintptr_t)&__userspace_start);
    }

    // Push onto the stack the variables
    THREAD_PUSH_STACK(REGS_SP(regs), uintptr_t, REGS_IP(regs));

    // !!!: Stupid hack to push flags
#if defined(__ARCH_I386__)
    THREAD_PUSH_STACK(REGS_SP(regs), uintptr_t, regs->eflags);
#elif defined(__ARCH_X86_64__)
    THREAD_PUSH_STACK(REGS_SP(regs), uintptr_t, regs->rflags);
#endif
    
    // Push handler and signal number
    THREAD_PUSH_STACK(REGS_SP(regs), uintptr_t, handler);
    THREAD_PUSH_STACK(REGS_SP(regs), uintptr_t, signum);

    // Set IP to point to the rebased signal handler
    uintptr_t signal_trampoline_offset = (uintptr_t)arch_signal_trampoline - (uintptr_t)&__userspace_start;
    REGS_IP(regs) = proc->userspace->base + signal_trampoline_offset;
    
    LOG(DEBUG, "Redirected IP to 0x%x\n", REGS_IP(regs));
    return 1;
} 

/**
 * @brief Handle signals sent to a process
 * @param thread The thread to check signals for
 * @param regs The current registers for the frame (this is called on IRQ)
 * @returns 0 on success
 */
int signal_handle(struct thread *thr, registers_t *regs) {
    process_t *proc = thr->parent;
    if (!proc) return 0;

    // Lock the process' siglock
    spinlock_acquire(&thr->siglock);
    if (!SIGNAL_ANY_PENDING(thr)) goto _done;
    
    for (int i = 0; i < NUMSIGNALS; i++) {
        if (SIGNAL_IS_PENDING(thr, i)) {
            // The signal is pending - let's handle it.
            int h = signal_try_handle(thr, i, regs);
            
            // Did we handle?
            if (h == 1) goto _done;
            if (h) {
                spinlock_release(&thr->siglock);
                return 1;
            }
        }
    }

_done:
    spinlock_release(&thr->siglock);
    return 0;
}

/**
 * @brief Send a signal to a group of processes
 * @param pgid The process group ID of the processes to send to
 * @param signal The signal to send to the group
 * @returns 0 on success, otherwise error code 
 */
int signal_sendGroup(pid_t pgid, int signal) {
    // TODO: Stupidity

extern list_t *process_list;
    foreach(node, process_list) {
        process_t *proc = node->value;
        if (proc->pgid == pgid) signal_send(proc, signal);
    }

    return 0;
}