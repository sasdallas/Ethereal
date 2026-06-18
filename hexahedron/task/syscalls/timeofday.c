/**
 * @file hexahedron/task/syscalls/gettimeofday.c
 * @brief gettimeofday
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

long sys_gettimeofday(struct timeval *tv, void *tz) {
    SYSCALL_VALIDATE_PTR(tv);
    if (tz) SYSCALL_VALIDATE_PTR(tz);
    return gettimeofday(tv, tz);
}

long sys_settimeofday(struct timeval *tv, void *tz) {
    SYSCALL_VALIDATE_PTR(tv);
    SYSCALL_VALIDATE_PTR(tz);
    return settimeofday(tv, tz);
}
