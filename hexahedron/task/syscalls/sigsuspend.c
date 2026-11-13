/**
 * @file hexahedron/task/syscalls/sigsuspend.c
 * @brief sigsuspend
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
#include <kernel/mm/vmm.h>

long sys_sigsuspend(const sigset_t *sigmask) {
    sigset_t mask = *sigmask;
    SIGNAL_CLR(mask, SIGKILL);
    SIGNAL_CLR(mask, SIGSTOP);

    thread_t *thread = current_cpu->current_thread;

    int irq_state = hal_setInterruptState(HAL_INTERRUPTS_DISABLED);
    spinlock_acquireRaw(&thread->signal.lock);

    thread->signal.suspend_saved = thread->signal.blocked;
    thread->signal.suspend_active = true;
    thread->signal.blocked = mask;
    __atomic_store_n(&thread->signal.have_pending, !!(thread->signal.pending & ~mask), __ATOMIC_RELEASE);

    sleep_prepareIRQ(irq_state);
    spinlock_releaseRaw(&thread->signal.lock);

    for (;;) {
        int reason = sleep_enter();
        if (reason == WAKEUP_SIGNAL) return -EINTR;
        sleep_prepare();
    }
}
