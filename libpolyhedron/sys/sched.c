/**
 * @file libpolyhedron/sys/sched.c
 * @brief sched stub functions
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <sched.h>
#include <sys/syscall.h>
#include <errno.h>

DEFINE_SYSCALL0(yield, SYS_YIELD);

int sched_get_priority_max(int policy) {
    return 0;
}

int sched_get_priority_min(int policy) {
    return 0;
}

int sched_yield() {
    __sets_errno(__syscall_yield());
}