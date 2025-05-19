/**
 * @file libpolyhedron/signal/kill.c
 * @brief kill signal
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
#include <sys/syscall_defs.h>
#include <signal.h>
#include <unistd.h>

DEFINE_SYSCALL2(kill, SYS_KILL, pid_t, int);

int kill(pid_t pid, int sig) {
    __sets_errno(__syscall_kill(pid, sig));
}

int raise(int sig) {
    return kill(getpid(), sig);
}