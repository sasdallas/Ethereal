/**
 * @file hexahedron/include/kernel/task/session.h
 * @brief Session/job manager
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#ifndef KERNEL_TASK_SESSION_H
#define KERNEL_TASK_SESSION_H

/**** INCLUDES ****/
#include <stdint.h>
#include <sys/types.h>
#include <structs/list.h>
#include <kernel/misc/spinlock.h>
#include <kernel/refcount.h>

/**** TYPES ****/

struct process;
struct session;
struct tty;

typedef struct process_group {
    pid_t pgid;
    struct session *session;
    DLIST_ENTRY(struct process_group) session_entry;
    spinlock_t lock;
    DLIST_HEAD(members, struct process);
    refcount_t refs;
} process_group_t;

typedef struct session {
    pid_t sid;
    spinlock_t lock;
    refcount_t refs;
    struct tty *ctty;
    DLIST_HEAD(groups, struct process_group);
} session_t;

/**** FUNCTIONS ****/

/**
 * @brief Initialize session control
 */
void session_init();

/**
 * @brief Initialize job control on a process
 * @param parent The parent of the process (NULL if init)
 * @param process The process
 */
void session_initProcess(struct process *parent, struct process *process);

/**
 * @brief Handle process destruction
 * @param process The dead process
 */
void session_destroyProcess(struct process *process);

/**
 * @brief Get the PGID
 */
pid_t session_getpgid(struct process *process);

/**
 * @brief Get the SID
 */
pid_t session_getsid(struct process *process);

/**
 * @brief Set controlling tty
 * @param tty The TTY to set
 * @param steal see @c TIOCSTTY
 */
int session_claimTTY(struct tty *tty, bool steal);

/**
 * @brief Hangup TTY
 * @param tty The TTY to hangup
 */
void session_hangup(struct tty *tty);

/**
 * @brief Signal group of processes
 * @param pgrp The process group to signal
 * @param signal The signal number
 * @note You should have a reference on pgrp on call
 */
int session_signalGroup(process_group_t *pgrp, int signal);

/**
 * @brief Signal foreground
 * @param tty The TTY to signal the foreground of
 * @param signal The signal number
 */
int session_signalForeground(struct tty *tty, int signal);

/**
 * @brief Get foreground (tcgetpgrp)
 * @param proc The process to get the foreground of
 * @param tty The TTY
 */
pid_t session_tcgetpgrp(struct process *proc, struct tty *tty);

/**
 * @brief Set foreground (tcsetpgrp)
 * @param proc The process to set the foreground of
 * @param tty The TTY to set the foreground on
 * @param pgid The process group ID
 */
int session_tcsetpgrp(struct process *proc, struct tty *tty, pid_t pgid);

/**
 * @brief Set pgid
 * @param pid Process ID
 * @param pgid Process group ID
 */
int session_setpgid(pid_t pid, pid_t pgid);

/**
 * @brief Set SID
 */
pid_t session_setsid();

#endif
