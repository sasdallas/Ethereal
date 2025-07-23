/**
 * @file libpolyhedron/sys/ptrace.c
 * @brief ptrace
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <sys/syscall.h>
#include <sys/ptrace.h>
#include <errno.h>

DEFINE_SYSCALL4(ptrace, SYS_PTRACE, enum __ptrace_request, pid_t, void*, void*);

long ptrace(enum __ptrace_request op, pid_t pid, void *addr, void *data) {
    __sets_errno(__syscall_ptrace(op, pid, addr, data));
}