/**
 * @file hexahedron/task/syscalls/setitimer.c
 * @brief setitimer
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


long sys_setitimer(int which, const struct itimerval *value, struct itimerval *ovalue) {
    if (which > ITIMER_PROF) return -EINVAL;

    // Check values
    if (value) SYSCALL_VALIDATE_PTR_SIZE(value, sizeof(struct itimerval));
    if (ovalue) SYSCALL_VALIDATE_PTR_SIZE(ovalue, sizeof(struct itimerval));

    if (!value && !ovalue) {
        return 0;
    }

    // Handle cases
    if (value) {
        if (ovalue) {
            // !!!: BUG HERE - it_value is not updated as the timer system sleeps. We can probably just do it here.
            ovalue->it_interval.tv_sec = current_cpu->current_process->itimers[which].reset_value.tv_sec;
            ovalue->it_interval.tv_usec = current_cpu->current_process->itimers[which].reset_value.tv_usec;
            ovalue->it_value.tv_sec = current_cpu->current_process->itimers[which].value.tv_sec;
            ovalue->it_value.tv_usec = current_cpu->current_process->itimers[which].value.tv_usec;
        }

        int r = timer_set(current_cpu->current_process, which, (struct itimerval*)value);
        if (r != 0) return r;
    } else {
        // They just want to get the timer in ovalue
            // !!!: BUG HERE - it_value is not updated as the timer system sleeps. We can probably just do it here.
        ovalue->it_interval.tv_sec = current_cpu->current_process->itimers[which].reset_value.tv_sec;
        ovalue->it_interval.tv_usec = current_cpu->current_process->itimers[which].reset_value.tv_usec;
        ovalue->it_value.tv_sec = current_cpu->current_process->itimers[which].value.tv_sec;
        ovalue->it_value.tv_usec = current_cpu->current_process->itimers[which].value.tv_usec;
    }


    return 0;
}
