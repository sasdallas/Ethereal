/**
 * @file hexahedron/task/syscalls/tkill.c
 * @brief tkill
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

long sys_tkill(pid_t tid, int sig) {
    if (sig < 0 || sig >= NSIG) return -EINVAL;

    process_t *proc = current_cpu->current_process;
    thread_t *target = NULL;

    spinlock_acquire(&proc->thread_lock);
    for (thread_t *thread = proc->thread_list; thread; thread = thread->next) {
        if (thread->tid == tid) {
            target = thread;
            break;
        }
    }
    spinlock_release(&proc->thread_lock);

    if (!target) return -ESRCH;
    if (!sig) return 0;

    signal_sendThread(target, sig);
    return 0;
}
