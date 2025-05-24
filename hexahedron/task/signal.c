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
    [SIGXFSZ]           = SIGNAL_ACTION_TERMINATE_CORE
};

/* Pending signal set */
#define SIGBIT(signum) (1 << signum)
#define SIGNAL_MARK_PENDING(proc, signum) (proc->pending_signals) |= SIGBIT(signum)
#define SIGNAL_UNMARK_PENDING(proc, signum) (proc->pending_signals) &= ~(SIGBIT(signum))
#define SIGNAL_IS_BLOCKED(proc, signum) (proc->blocked_signals & SIGBIT(signum) && !(signum == SIGKILL) && !(signum == SIGSTOP))
#define SIGNAL_IS_PENDING(proc, signum) (proc->pending_signals & SIGBIT(signum) && !SIGNAL_IS_BLOCKED(proc, signum))
#define SIGNAL_ANY_PENDING(proc) (proc->pending_signals & ~proc->blocked_signals)

/* Log method */
#define LOG(status, ...) dprintf_module(status, "TASK:SIGNAL", __VA_ARGS__)

/**
 * @brief Send a signal to a process
 * @param proc The process to send the signal to
 * @param signal The signal to send to the process
 * @returns 0 on success, otherwise error code
 */
int signal_send(struct process *proc, int signal) {
    if (signal < 0 || signal >= NUMSIGNALS) return -EINVAL;

    // Are they trying to continue a process?
    if (PROCESS_SIGNAL(proc, signal).handler == SIGNAL_ACTION_CONTINUE && proc->flags & PROCESS_SLEEPING) {
        // TODO: Continue
        LOG(ERR, "Cannot continue a process as this is unimplemented\n");
        return -ENOTSUP;
    }

    // Is this signal blocked?
    if (SIGNAL_IS_BLOCKED(proc, signal)) {
        return 0;
    }

    // Do we just ignore it? Don't waste time on that
    if (PROCESS_SIGNAL(proc, signal).handler == SIGNAL_ACTION_IGNORE) {
        return 0;
    }

    // Mark the signal as pending
    spinlock_acquire(&proc->siglock);
    LOG(DEBUG, "Sending signal %d to process pid %d\n", signal, proc->pid);
    SIGNAL_MARK_PENDING(proc, signal);
    spinlock_release(&proc->siglock);

    // TODO: Interrupt system calls

    // Wake them up if they aren't us
    if (proc != current_cpu->current_process && (proc->flags & PROCESS_STOPPED || proc->flags & PROCESS_SLEEPING)) {
        // TODO: Continue
        LOG(ERR, "Cannot wake up a process as this is unimplemented\n");
        return -ENOTSUP;
    }

    return 0;
}

/**
 * @brief Internal method to handle signal
 * @returns 1 if the signal was handled and to return, 0 if not handled
 */
static int signal_try_handle(thread_t *thr, int signum, registers_t *regs) {
    process_t *proc = thr->parent;
    if (!proc) return 0;

    // Get signal and handler
    proc_signal_t *sig = &PROCESS_SIGNAL(proc, signum);
    sa_handler handler = (sig->handler ? sig->handler : signal_default_action[signum]);

    // Reset?
    if (sig->flags & SA_RESETHAND) {
        sig->handler = SIGNAL_ACTION_DEFAULT;
    }

    // We're gonna handle this signal so unset it
    SIGNAL_UNMARK_PENDING(proc, signum);

    // Handle appropriately
    if (handler == SIGNAL_ACTION_DEFAULT) {
        kernel_panic_extended(UNKNOWN_CORRUPTION_DETECTED, "signal", "*** The default signal handler says to call the default signal handler.\n");
    } else if (handler == SIGNAL_ACTION_CONTINUE) {
        // TODO: Continue
        LOG(ERR, "Cannot continue process as this is unimplemented\n");
        return -ENOTSUP;
    } else if (handler == SIGNAL_ACTION_STOP) {
        SLEEP_ENTIRE_PROCESS(proc, sleep_untilNever(t));
    } else if (handler == SIGNAL_ACTION_IGNORE) {
        // Ignore signal
        return 0;
    } else if (handler == SIGNAL_ACTION_TERMINATE || handler == SIGNAL_ACTION_TERMINATE_CORE) {
        // Terminate the process
        process_exit(proc, ((128 + signum) << 8) | signum);
        return 2;
    }

    // Else, let's have it call the handler.
    LOG(DEBUG, "Handling signal %d for process PID %d (handler: %p)\n", signum, proc->pid, handler);


extern uintptr_t __userspace_start, __userspace_end;

    // If the process does not have a userspace allocation, create one.
    // !!!: This probably needs to be refactored?
    if (!proc->userspace) {
        proc->userspace = vas_allocate(proc->vas, PAGE_SIZE);
        if (!proc->userspace) {
            kernel_panic_extended(OUT_OF_MEMORY, "signal", "*** Out of memory when allocating a signal trampoline.\n");
        }
        
        // Copy in the userspace section
        memcpy((void*)proc->userspace->base, &__userspace_start, (uintptr_t)&__userspace_end - (uintptr_t)&__userspace_start);
    }

    // Push onto the stack the variables
    THREAD_PUSH_STACK(REGS_SP(regs), uintptr_t, handler);
    THREAD_PUSH_STACK(REGS_SP(regs), uintptr_t, signum);
    THREAD_PUSH_STACK(REGS_SP(regs), uintptr_t, REGS_IP(regs));

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
    spinlock_acquire(&proc->siglock);
    if (!SIGNAL_ANY_PENDING(proc)) goto _done;
    
    for (int i = 0; i < NUMSIGNALS; i++) {
        if (SIGNAL_IS_PENDING(proc, i)) {
            // The signal is pending - let's handle it.
            int h = signal_try_handle(thr, i, regs);
            
            // Did we handle?
            if (h == 1) goto _done;
            if (h) {
                spinlock_release(&proc->siglock);
                return 1;
            }
        }
    }

_done:
    spinlock_release(&proc->siglock);
    return 0;
}