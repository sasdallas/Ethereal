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
#include <stdarg.h>

DEFINE_SYSCALL4(ptrace, SYS_PTRACE, enum __ptrace_request, pid_t, void*, void*);

long ptrace(enum __ptrace_request op, ...) {
    va_list ap;
    va_start(ap, op);
    
    // Get arguments
    pid_t pid = va_arg(ap, pid_t);
    void *addr = va_arg(ap, void*);
    void *data = va_arg(ap, void*);

    // ptrace
    long ret = __syscall_ptrace(op, pid, addr, data);
    va_end(ap);
    
    __sets_errno(ret);
}