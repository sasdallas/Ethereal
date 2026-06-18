/**
 * @file hexahedron/task/syscalls/times.c
 * @brief times
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

clock_t sys_times(struct tms *buf) {
    // sum up all process times
    process_t *p = current_cpu->current_process;

    buf->tms_stime = 0;
    buf->tms_utime = 0;
    spinlock_acquire(&p->thread_lock);
    thread_t *iter = p->thread_list;
    while (iter) {
        buf->tms_stime += iter->times.stime;
        buf->tms_utime += iter->times.utime;
        iter = iter->next;
    }
    spinlock_release(&p->thread_lock);

    buf->tms_cstime = p->proc_times.cstime;
    buf->tms_cutime = p->proc_times.cutime;

extern uint64_t timemonitor_getNanoseconds();
    return (clock_t)timemonitor_getNanoseconds();
}
