/**
 * @file hexahedron/include/kernel/task/signal.h
 * @brief Signal handler daemon
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#ifndef KERNEL_TASK_SIGNAL2_H
#define KERNEL_TASK_SIGNAL2_H

/**** INCLUDES ****/
#include <stdint.h>
#include <sys/signal.h>

/**** DEFINITIONS ****/

/* mlibc requires definitions that the kernel doesn't provide for these */
#ifndef SS_ONSTACK
#define SS_ONSTACK  1
#endif
#ifndef SS_DISABLE
#define SS_DISABLE  2
#endif
#ifndef MINSIGSTKSZ
#define MINSIGSTKSZ 2048
#endif

/**** TYPES ****/

typedef struct signal_action {
    uintptr_t handler;
    sigset_t mask;
    int flags;
    void *restorer;
} signal_action_t;


struct process;
struct thread;

/**** MACROS ****/

#define SIGNAL_BIT(signum) (((sigset_t)1) << ((signum) - 1))
#define SIGNAL_GET(mask, signal) (!!((mask) & SIGNAL_BIT(signal)))
#define SIGNAL_SET(mask, signal) (mask) |= (SIGNAL_BIT(signal))
#define SIGNAL_CLR(mask, signal) (mask) &= ~(SIGNAL_BIT(signal))
#define SIGNAL_BLOCKED(thread, sig) SIGNAL_GET((thread)->signal.blocked, sig)
#define SIGNAL_IGNORABLE(signal) ((signal) != SIGSTOP && (signal) != SIGKILL)

/**** FUNCTIONS ****/

/**
 * @brief Send a signal to a process
 * @param proc The process to signal
 * @param signal The signal to send
 * @param info Signal information (optional, leave as NULL to not provide)
 */
void signal_sendInfo(struct process *proc, int signal, siginfo_t *info);

/**
 * @brief Send a signal to a process
 * @param proc The process to signal
 * @param signal The signal to send
 */
static inline void signal_send(struct process *proc, int signal) {
    return signal_sendInfo(proc, signal, NULL);
}

/**
 * @brief Send a signal to a thread
 * @param thread The thread to signal
 * @param signal The signal to send
 * @param info Signal information (optional, leave as NULL to not provide)
 */
void signal_sendThreadInfo(struct thread *thread, int signal, siginfo_t *info);

/**
 * @brief Send a signal to a thread
 * @param thread The thread to signal
 * @param signal The signal to send
 */
static inline void signal_sendThread(struct thread *thread, int signal) {
    return signal_sendThreadInfo(thread, signal, NULL);
}

/**
 * @brief Send a signal to a group of processes
 * @param pgid The process group ID of the processes to send to
 * @param signal The signal to send to the group
 * @returns 0 on success, otherwise error code 
 */
int signal_sendGroup(pid_t pgid, int signal);

/**
 * @brief sigprocmask
 */
int signal_procmask(int how, const sigset_t *set, sigset_t *oset);

/**
 * @brief sigaction
 */
int signal_action(int signal, struct sigaction *new, struct sigaction *old);

/**
 * @brief Check signal on return
 * @param regs Returning trap frame
 */
void signal_check(registers_t *regs);

/**
 * @brief Check whether an address is on the alternate stack
 * @param thread The thread to check
 * @param addr The address to check
 */
bool signal_onAltStack(struct thread *thread, uintptr_t addr);

/**
 * @brief sigaltstack
 */
int signal_sigaltstack(const stack_t *ss, stack_t *oss);

#endif
