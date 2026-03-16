/**
 * @file hexahedron/task/syscalls/futex.c
 * @brief futex syscalls
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/task/process.h>

long sys_futex_wait(uint32_t *pointer, uint32_t val, const struct timespec *time) {
    SYSCALL_VALIDATE_PTR(pointer);
    if (time) SYSCALL_VALIDATE_PTR(time);

    return futex_wait(pointer, val, time);
}

long sys_futex_wake(uint32_t *pointer) {
    SYSCALL_VALIDATE_PTR(pointer);
    return futex_wakeup(pointer, INT_MAX);
}
