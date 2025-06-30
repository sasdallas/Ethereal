/**
 * @file libpolyhedron/signal/sigprocmask.c
 * @brief sigprocmask
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <signal.h>
#include <sys/syscall.h>
#include <errno.h>

DEFINE_SYSCALL3(sigprocmask, SYS_SIGPROCMASK, int, const sigset_t*, sigset_t*);

int sigprocmask(int how, const sigset_t *set, sigset_t *oldset) {
    __sets_errno(__syscall_sigprocmask(how, set, oldset));
}