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
#include <errno.h>
#include <stdint.h>

long sys_setitimer(int which, const struct itimerval *value, struct itimerval *ovalue) {
    itimer_t *timer = NULL;
    switch (which) {
        case ITIMER_REAL: timer = &current_cpu->current_process->timer.real; break;
        case ITIMER_VIRTUAL: timer = &current_cpu->current_process->timer.virtual; break;
        case ITIMER_PROF: timer = &current_cpu->current_process->timer.prof; break;
        default: return -EINVAL;
    }

    if (value) {
        // snapshot here, since itimer_set modifies it
        struct itimerval requested = *value;
        itimer_set(timer, &requested.it_value, &requested.it_interval);

        if (ovalue) {
            ovalue->it_value = requested.it_value;
            ovalue->it_interval = requested.it_interval;
        }
    } else if (ovalue) {
        itimer_get(timer, &ovalue->it_value, &ovalue->it_interval);
    }

    return 0;
}
