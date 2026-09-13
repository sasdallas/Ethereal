/**
 * @file hexahedron/task/syscalls/ptrace.c
 * @brief ptrace
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

long sys_ptrace(enum __ptrace_request op, pid_t pid, void *addr, void *data) {
    SYSCALL_LOG(ERR, "sys_ptrace is not enabled\n");
    return -ENOSYS;
    // return ptrace_handle(op, pid, addr, data);
}
