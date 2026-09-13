/**
 * @file hexahedron/task/syscalls/sigreturn.c
 * @brief sigreturn
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#include <kernel/arch/arch.h>
#include <kernel/task/process.h>

long sys_sigreturn(void *uctx) {
    thread_t *thr = current_cpu->current_thread;

    // restore signal frame into returning thread's registers
    int r = arch_restore_signal_frame(thr, thr->regs, uctx);
    if (r < 0) {
        SYSCALL_LOG(ERR, "Bad signal frame at %p, terminating pid %d\n", uctx, thr->parent->pid);
        process_exit(thr->parent, 139);
        return r;
    }

    // get back from it
    ucontext_t *context = uctx;
    spinlock_acquire(&thr->signal.lock);
    thr->signal.blocked = context->uc_sigmask;
    SIGNAL_CLR(thr->signal.blocked, SIGKILL); // uc_sigmask cannot block these
    SIGNAL_CLR(thr->signal.blocked, SIGSTOP);
    __atomic_store_n(&thr->signal.have_pending, !!(thr->signal.pending & ~thr->signal.blocked), __ATOMIC_RELEASE);
    spinlock_release(&thr->signal.lock);

#ifdef __ARCH_X86_64__
    // !!! BAD force IRET hack
    // this is because the SYSRETQ instruction won't restore all registers
    if (thr->syscall) {
        thr->syscall->force_iret = 1;
    }
#endif

    return (long)thr->regs->rax;
}
