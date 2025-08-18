/**
 * @file hexahedron/include/kernel/task/signal.h
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

#ifndef KERNEL_TASK_SIGNAL_H
#define KERNEL_TASK_SIGNAL_H

/**** INCLUDES ****/
#include <stdint.h>
#include <signal.h>

/**** DEFINITIONS ****/

/* Signal actions */
#define SIGNAL_ACTION_DEFAULT               (void*)0 // Kernel clears array to 0, so this works out nice
#define SIGNAL_ACTION_TERMINATE             (void*)1
#define SIGNAL_ACTION_TERMINATE_CORE        (void*)2
#define SIGNAL_ACTION_IGNORE                (void*)3
#define SIGNAL_ACTION_STOP                  (void*)4
#define SIGNAL_ACTION_CONTINUE              (void*)5

/**** TYPES ****/

typedef struct proc_signal {
    void        (*handler)(int);        // Signal handler
    sigset_t    mask;           // Signal mask
    int         flags;          // Flags
} proc_signal_t;

typedef void *__signal_handler;

/**** MACROS ****/

#define THREAD_SIGNAL(thr, signum) (((thr)->signals[(signum)]))

/**** FUNCTIONS ****/

struct process;

/**
 * @brief Send a signal to a process
 * @param proc The process to send the signal to
 * @param signal The signal to send to the process
 * @returns 0 on success, otherwise error code
 */
int signal_send(struct process *proc, int signal);

/**
 * @brief Handle signals sent to a process
 * @param thread The thread to check signals for
 * @param regs The current registers for the frame (this is called on IRQ)
 * @returns 0 on success
 */
int signal_handle(struct thread *thr, struct _registers *regs);

/**
 * @brief Send a signal to a group of processes
 * @param pgid The process group ID of the processes to send to
 * @param signal The signal to send to the group
 * @returns 0 on success, otherwise error code 
 */
int signal_sendGroup(pid_t pgid, int signal);


#endif