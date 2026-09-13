/**
 * @file hexahedron/task/syscalls/usleep.c
 * @brief usleep
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

long sys_usleep(useconds_t usec) {
    sleep_time((usec / 1000000), (usec % 1000000));
    if (sleep_enter() == WAKEUP_SIGNAL) return -EINTR;

    return 0;
}
