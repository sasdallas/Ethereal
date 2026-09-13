/**
 * @file hexahedron/task/signal.c
 * @brief Signal subsystem
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#include <kernel/task/process.h>
#include <kernel/misc/spinlock.h>
#include <kernel/mm/vmm.h>
#include <kernel/debug.h>
#include <errno.h>

/* Default actions */
#define SIGNAL_TERMINATE 0
#define SIGNAL_TERMINATE_CORE 1
#define SIGNAL_IGNORE 2
#define SIGNAL_CONTINUE 3
#define SIGNAL_STOP 4

int signal_default_actions[] = {
    [SIGHUP]    = SIGNAL_TERMINATE,
    [SIGINT]    = SIGNAL_TERMINATE,
    [SIGQUIT]   = SIGNAL_TERMINATE_CORE,
    [SIGILL]    = SIGNAL_TERMINATE_CORE,
    [SIGTRAP]   = SIGNAL_TERMINATE_CORE,
    [SIGABRT]   = SIGNAL_TERMINATE_CORE,
    [SIGBUS]    = SIGNAL_TERMINATE_CORE,
    [SIGFPE]    = SIGNAL_TERMINATE_CORE,
    [SIGKILL]   = SIGNAL_TERMINATE,
    [SIGUSR1]   = SIGNAL_TERMINATE,
    [SIGSEGV]   = SIGNAL_TERMINATE_CORE,
    [SIGUSR2]   = SIGNAL_TERMINATE,
    [SIGPIPE]   = SIGNAL_TERMINATE,
    [SIGALRM]   = SIGNAL_TERMINATE,
    [SIGTERM]   = SIGNAL_TERMINATE,
    [SIGSTKFLT] = SIGNAL_TERMINATE,
    [SIGCHLD]   = SIGNAL_IGNORE,
    [SIGCONT]   = SIGNAL_CONTINUE,
    [SIGSTOP]   = SIGNAL_STOP,
    [SIGTSTP]   = SIGNAL_STOP,
    [SIGTTIN]   = SIGNAL_STOP,
    [SIGTTOU]   = SIGNAL_STOP,
    [SIGURG]    = SIGNAL_IGNORE,
    [SIGXCPU]   = SIGNAL_TERMINATE_CORE,
    [SIGXFSZ]   = SIGNAL_TERMINATE_CORE,
    [SIGVTALRM] = SIGNAL_TERMINATE,
    [SIGPROF]   = SIGNAL_TERMINATE,
    [SIGWINCH]  = SIGNAL_IGNORE,
    [SIGPOLL]   = SIGNAL_TERMINATE,
    [SIGPWR]    = SIGNAL_TERMINATE,
    [SIGSYS]    = SIGNAL_TERMINATE_CORE,
    [SIGCANCEL] = SIGNAL_IGNORE, // ???
    [SIGTIMER]  = SIGNAL_IGNORE, // ???
    SIGNAL_TERMINATE
};


/* Log method */
#define LOG(status, ...) dprintf_module(status, "TASK:SIGNAL", __VA_ARGS__)

/**
 * @brief Find a valid thread to route the signal to
 * @param proc The process being signalled
 * @param signal The signal to route
 */
static thread_t *signal_findThread(process_t *proc, int signal) {
    spinlock_acquire(&proc->thread_lock);
    thread_t *thr = proc->thread_list;
    while (thr) {
        if (thr->status & (THREAD_STATUS_STOPPED | THREAD_STATUS_STOPPING)) {
            thr = thr->next;
            continue;
        }

        spinlock_acquire(&thr->signal.lock);
        if (!SIGNAL_BLOCKED(thr, signal) || !SIGNAL_IGNORABLE(signal) || signal == SIGCONT) {
            break;
        }
        spinlock_release(&thr->signal.lock);

        thr = thr->next;
    }
    spinlock_release(&proc->thread_lock);

    return thr;
}

/**
 * @brief Kick a thread after signalling it
 */
static void signal_kick(thread_t *thread) {
    __atomic_store_n(&thread->signal.have_pending, true, __ATOMIC_RELEASE);
    if (thread->status & THREAD_STATUS_SLEEPING) {
        sleep_wakeupReason(thread, WAKEUP_SIGNAL);
    }
}

/**
 * @brief Send a signal to a process
 * @param proc The process to signal
 * @param signal The signal to send
 * @param info Signal information (optional, leave as NULL to not provide)
 */
void signal_sendInfo(process_t *proc, int signal, siginfo_t *info) {
    if (signal < 0 && signal >= SIGRTMIN) {
        LOG(ERR, "Unsupported signal: %d\n", signal);
        return;
    }

    assert(info == NULL && "Info is not supported yet on process-wide signals");

    spinlock_acquire(&proc->signal.lock);

    signal_action_t *act = &proc->signal.actions[signal];
    bool ignored = (act->handler == (uintptr_t)SIG_IGN) || (act->handler == (uintptr_t)SIG_DFL && signal_default_actions[signal] == SIGNAL_IGNORE);
    
    // LOG(INFO, "send signal %d (ignored: %d) to process %d\n", signal, ignored, proc->pid);

    bool resume = false;
    thread_t *route = NULL;

    if (signal == SIGCONT) {
        resume = proc->state == PROCESS_SUSPENDED;
        proc->state = PROCESS_RUNNING;
        if (resume) {
            __atomic_store_n(&proc->continued, true, __ATOMIC_SEQ_CST);
        }

        // hack, if being continued these signals need to be cleared
        SIGNAL_CLR(proc->signal.pending, SIGSTOP);
        SIGNAL_CLR(proc->signal.pending, SIGTSTP);
        SIGNAL_CLR(proc->signal.pending, SIGTTIN);
        SIGNAL_CLR(proc->signal.pending, SIGTTOU);
    }

    if (ignored && SIGNAL_IGNORABLE(signal) && signal != SIGCONT) {
        goto _leave;
    }

    route = signal_findThread(proc, signal);

    if (route == NULL) {
        // Defer to next waking thread
        // TODO: Find threads waiting for signals when support is impl.d
        SIGNAL_SET(proc->signal.pending, signal);
        __atomic_store_n(&proc->signal.have_pending, true, __ATOMIC_RELEASE);
    } else {
        // Kick this poor sucker
        SIGNAL_SET(route->signal.pending, signal);
        signal_kick(route);
        spinlock_release(&route->signal.lock);
    }

_leave:
    spinlock_release(&proc->signal.lock);

    if (resume && proc->parent) {
        signal_send(proc->parent, SIGCHLD);
        EVENT_SIGNAL(&proc->parent->wait_event);
    }

    if (resume && route && route != current_cpu->current_thread && !(route->status & (THREAD_STATUS_SLEEPING | THREAD_STATUS_STOPPING | THREAD_STATUS_STOPPED))) {
        sched_insert(route);
    }

    return;

}

/**
 * @brief Send a signal to a thread
 * @param thread The thread to signal
 * @param signal The signal to send
 * @param info Signal information (optional, leave as NULL to not provide)
 */
void signal_sendThreadInfo(thread_t *thread, int signal, siginfo_t *info) {
    assert(signal > 0 && signal < SIGRTMIN && "Invalid or unsupported signal");

    process_t *proc = thread->parent;
    spinlock_acquire(&proc->signal.lock);
    spinlock_acquire(&thread->signal.lock);
    
    bool resume = false;
    if (signal == SIGCONT) {
        resume = (proc->state == PROCESS_SUSPENDED);
        proc->state = PROCESS_RUNNING;
        if (resume) __atomic_store_n(&proc->continued, true, __ATOMIC_SEQ_CST);
        
        // hack, if being continued these signals need to be cleared
        SIGNAL_CLR(thread->signal.pending, SIGSTOP);
        SIGNAL_CLR(thread->signal.pending, SIGTSTP);
        SIGNAL_CLR(thread->signal.pending, SIGTTIN);
        SIGNAL_CLR(thread->signal.pending, SIGTTOU);
    }

    signal_action_t *act = &proc->signal.actions[signal];
    bool ignored = (act->handler == (uintptr_t)SIG_IGN) || (act->handler == (uintptr_t)SIG_DFL && signal_default_actions[signal] == SIGNAL_IGNORE);
    if (!SIGNAL_IGNORABLE(signal)) ignored = false;

    if (ignored) {
        goto _leave;
    }

    if (info && !SIGNAL_GET(thread->signal.pending, signal)) {
        memcpy(&thread->signal.info[signal], info, sizeof(siginfo_t));
    }

    SIGNAL_SET(thread->signal.pending, signal);
    if (!SIGNAL_BLOCKED(thread, signal)) {
        signal_kick(thread);
    }

_leave:
    spinlock_release(&thread->signal.lock);
    spinlock_release(&proc->signal.lock);

    if (resume && proc->parent) {
        signal_send(proc->parent, SIGCHLD);
        EVENT_SIGNAL(&proc->parent->wait_event);
    }

    if (resume && thread != current_cpu->current_thread && !(thread->status & (THREAD_STATUS_SLEEPING | THREAD_STATUS_STOPPING | THREAD_STATUS_STOPPED))) {
        sched_insert(thread);
    }
}

/**
 * @brief Send a signal to a group of processes
 * @param pgid The process group ID of the processes to send to
 * @param signal The signal to send to the group
 * @returns 0 on success, otherwise error code 
 */
int signal_sendGroup(pid_t pgid, int signal) {
    // TODO: Stupidity

    // !!! VERY UNSAFE WALK!!
extern list_t *process_list;
    foreach(node, process_list) {
        process_t *proc = node->value;
        if (proc->pgid == pgid) {
            signal_send(proc, signal);
        }
    }

    return 0;
}

/**
 * @brief sigprocmask
 */
int signal_procmask(int how, const sigset_t *set, sigset_t *oset) {
    thread_t *thr = current_cpu->current_thread;

    // TODO: introduce a usercopy system.. usermode memory cannot be accessed from this unsafe IRQs off context
    sigset_t new;
    if (set) {
        new = *set;

        // SIGKILL and SIGSTOP may not be blocked.
        SIGNAL_CLR(new, SIGKILL);
        SIGNAL_CLR(new, SIGSTOP);
    }

    // entering IRQ context
    spinlock_acquire(&thr->signal.lock);
    sigset_t og = thr->signal.blocked;
    
    if (set) {
        switch (how) {
            case SIG_BLOCK: thr->signal.blocked |= new; break;
            case SIG_UNBLOCK: thr->signal.blocked &= ~(new); break;
            case SIG_SETMASK: thr->signal.blocked = new; break;

            default:
                spinlock_release(&thr->signal.lock);
                return -EINVAL;
        }
    }

    bool pending = !!(thr->signal.pending & ~thr->signal.blocked);
    __atomic_store_n(&thr->signal.have_pending, pending, __ATOMIC_RELEASE);

    // exiting IRQ context
    spinlock_release(&thr->signal.lock);

    if (oset) *oset = og;
    return 0;
}

/**
 * @brief sigaction
 */
int signal_action(int signal, struct sigaction *new, struct sigaction *old) {
    process_t *proc = current_cpu->current_process;
    if (signal <= 0 || signal >= _NSIG) return -EINVAL;
    if (new && (signal == SIGKILL || signal == SIGSTOP)) return -EINVAL;

    // TODO: introduce a usercopy system.. usermode memory cannot be accessed from this unsafe IRQs off context
    struct sigaction saved_new;
    if (new) saved_new = *new;

    spinlock_acquire(&proc->signal.lock);
    signal_action_t *act = &proc->signal.actions[signal];
    signal_action_t saved = *act;
    if (new) {
        act->flags = saved_new.sa_flags;
        act->mask = saved_new.sa_mask;
        act->restorer = saved_new.sa_restorer;
        act->handler = (uintptr_t)saved_new.sa_handler;
    }
    spinlock_release(&proc->signal.lock);

    if (old) {
        old->sa_flags = saved.flags;
        old->sa_mask = saved.mask;
        old->sa_restorer = saved.restorer;
        old->sa_handler = (void (*)(int))saved.handler;
    }

    return 0;
}

/**
 * @brief sigaltstack
 */
int signal_sigaltstack(const stack_t *ss, stack_t *oss) {
    thread_t *thr = current_cpu->current_thread;

    // Possible non-present access, todo usercopy, yada yada.
    stack_t restore;
    if (ss) restore = *ss;

    spinlock_acquire(&thr->signal.lock);

    bool on_alt_stack = signal_onAltStack(thr, thr->regs->rip);
    
    stack_t saved = thr->signal.altstack;
    
    // Fix ss_flags
    if (saved.ss_size) {
        saved.ss_flags = (on_alt_stack) ? SS_ONSTACK : 0;
    } else {
        saved.ss_flags = SS_DISABLE;
    }

    if (ss) {
        // POSIX says this is illegal (for good reason)
        if (on_alt_stack) {
            spinlock_release(&thr->signal.lock);
            return -EPERM;
        }

        if (restore.ss_flags & SS_DISABLE) {
            memset(&thr->signal.altstack, 0, sizeof(stack_t));
        } else {
            if (restore.ss_size < MINSIGSTKSZ) {
                spinlock_release(&thr->signal.lock);
                return -ENOMEM;
            }

            thr->signal.altstack = restore;
            thr->signal.altstack.ss_flags = 0;
        }
    }

    spinlock_release(&thr->signal.lock);

    if (oss) *oss = saved;
    return 0;
}

/**
 * @brief Check signal on return
 * @param regs Returning trap frame
 */
void signal_check(registers_t *regs) {
    process_t *proc = current_cpu->current_process;
    thread_t *thr = current_cpu->current_thread;

    if (!__atomic_load_n(&thr->signal.have_pending, __ATOMIC_ACQUIRE)) {
        if (LIKELY(!__atomic_load_n(&proc->signal.have_pending, __ATOMIC_ACQUIRE))) {
            return;
        }
    }

    spinlock_acquire(&proc->signal.lock);
    spinlock_acquire(&thr->signal.lock);

_retry:

    // Check for SIGKILL first as it has priority
    if (SIGNAL_GET(thr->signal.pending, SIGKILL) || SIGNAL_GET(proc->signal.pending, SIGKILL)) {
        spinlock_release(&thr->signal.lock);
        spinlock_release(&proc->signal.lock);
        process_exit(proc, SIGKILL);
        __builtin_unreachable();
    }

    // Check thread lists to find the first signal to process
    int target_signal = -1;
    for (int i = 1; i < SIGRTMIN; i++) {
        if (SIGNAL_BLOCKED(thr, i)) continue;

        if (SIGNAL_GET(thr->signal.pending, i)) {
            target_signal = i;
            SIGNAL_CLR(thr->signal.pending, i);
            break;
        }
    }

    if (target_signal == -1) {
        // Check process list
        for (int i = 1; i < SIGRTMIN; i++) {
            if (SIGNAL_BLOCKED(thr, i)) continue;
            if (SIGNAL_GET(proc->signal.pending, i)) {
                target_signal = i;
                SIGNAL_CLR(proc->signal.pending, i);
                break;
            }
        }

        if (target_signal == -1) {
            // Lost race
            __atomic_store_n(&proc->signal.have_pending,
                    proc->signal.pending != 0, __ATOMIC_RELEASE);
            __atomic_store_n(&thr->signal.have_pending,
                    !!(thr->signal.pending & ~thr->signal.blocked), __ATOMIC_RELEASE);
            
            goto _leave;
        }
    }

    // If we found something
    LOG(INFO, "Found signal %d to be processed\n", target_signal);

    signal_action_t *act = &proc->signal.actions[target_signal];

    if (act->handler == (uintptr_t)SIG_IGN) {
        goto _retry;
    } else if (act->handler == (uintptr_t)SIG_DFL) {
        int action = signal_default_actions[target_signal];

        if (action == SIGNAL_IGNORE || action == SIGNAL_CONTINUE) {
            goto _retry;
        } else if (action == SIGNAL_TERMINATE || action == SIGNAL_TERMINATE_CORE) {
            spinlock_release(&thr->signal.lock);
            spinlock_release(&proc->signal.lock);

            // interrupts must be on to terminate a process
            hal_setInterruptState(HAL_INTERRUPTS_ENABLED);
            process_exit(proc, target_signal);
        } else if (action == SIGNAL_STOP) {
            proc->exit_status = target_signal;
            proc->exit_reason = PROCESS_EXIT_SIGNAL;
            proc->state = PROCESS_SUSPENDED;

            spinlock_release(&thr->signal.lock);
            spinlock_release(&proc->signal.lock);

            if (proc->parent) {
                signal_send(proc->parent, SIGCHLD);
                EVENT_SIGNAL(&proc->parent->wait_event);
            }

            process_yield(0);
            return;
        } else {
            assert(0 && "bad signal_default_action");
        }
    }

    signal_action_t saved_action = *act;
    sigset_t saved_mask = thr->signal.blocked;

    // The signal stays blocked for the duration of the handler
    thr->signal.blocked |= saved_action.mask;
    if (!(saved_action.flags & SA_NODEFER)) {
        SIGNAL_SET(thr->signal.blocked, target_signal);
    }

    if (saved_action.flags & SA_RESETHAND) {
        memset(act, 0, sizeof(signal_action_t));
    }

    // re-evaluate have_pending on these
    __atomic_store_n(&proc->signal.have_pending, proc->signal.pending != 0, __ATOMIC_RELEASE);
    __atomic_store_n(&thr->signal.have_pending, !!(thr->signal.pending & ~thr->signal.blocked), __ATOMIC_RELEASE);

    spinlock_release(&thr->signal.lock);
    spinlock_release(&proc->signal.lock);

    // because this memory can be pageable, interrupts are required to be enabled
    // (arch_prepare_signal_frame is an abomination)
    hal_setInterruptState(HAL_INTERRUPTS_ENABLED);
    if (arch_prepare_signal_frame(thr, target_signal, regs, saved_action.handler, saved_action.restorer, saved_action.flags, saved_mask) != 0) {
        LOG(ERR, "arch_prepare_signal_frame did not succeed for signal %d\n", target_signal);
        process_exit(proc, target_signal);
    }

    if (thr->syscall) {
        thr->syscall->force_iret = 1;
    }

    return;

_leave:
    __atomic_store_n(&proc->signal.have_pending, proc->signal.pending != 0, __ATOMIC_RELEASE);
    __atomic_store_n(&thr->signal.have_pending, !!(thr->signal.pending & ~thr->signal.blocked), __ATOMIC_RELEASE);
    spinlock_release(&thr->signal.lock);
    spinlock_release(&proc->signal.lock);
}

/**
 * @brief Check whether an address is on the alternate stack
 * @param thread The thread to check
 * @param addr The address to check
 * @warning Assumes thread signal lock is already held
 */
bool signal_onAltStack(thread_t *thread, uintptr_t addr) {
    if (thread->signal.altstack.ss_size == 0) {
        return false;
    }

    uintptr_t stk_base = (uintptr_t)thread->signal.altstack.ss_sp;
    uintptr_t stk_end = stk_base + (thread->signal.altstack.ss_size);

    return (addr >= stk_base && addr < stk_end);
}
