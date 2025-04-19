/**
 * @file libpolyhedron/unistd/execve.c
 * @brief execve
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of reduceOS.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <unistd.h>
#include <sys/syscall.h>

DEFINE_SYSCALL3(execve, SYS_EXECVE, const char*, const char**, char**);

int execve(const char *pathname, const char *argv[], char *envp[]) {
    __sets_errno(__syscall_execve(pathname, argv, envp));
}

int execv(const char *path, const char *argv[]) {
    return execve(path, argv, environ);
}