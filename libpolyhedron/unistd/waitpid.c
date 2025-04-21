/**
 * @file libpolyhedron/unistd/waitpid.c
 * @brief waitpid
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <sys/wait.h>
#include <sys/syscall.h>
#include <unistd.h>

DEFINE_SYSCALL3(wait, SYS_WAIT, pid_t, int*, int);

pid_t waitpid(pid_t pid, int *wstatus, int options) {
    __sets_errno(__syscall_wait(pid, wstatus, options));
}

pid_t wait(int *wstatus) {
    return waitpid(-1, wstatus, 0);
}