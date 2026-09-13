/**
 * @file hexahedron/task/syscalls/sigpending.c
 * @brief sigpending
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

long sys_sigpending(sigset_t *set) {
    thread_t *thr = current_cpu->current_thread;
    process_t *proc = current_cpu->current_process;

    spinlock_acquire(&proc->signal.lock);
    spinlock_acquire(&thr->signal.lock);
    *set = thr->signal.pending | proc->signal.pending;
    spinlock_release(&thr->signal.lock);
    spinlock_release(&proc->signal.lock);
    return 0;
}
