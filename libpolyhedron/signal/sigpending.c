/**
 * @file libpolyhedron/signal/sigpending.c
 * @brief sigpending
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
#include <signal.h>
#include <errno.h>

DEFINE_SYSCALL1(sigpending, SYS_SIGPENDING, sigset_t*);

int sigpending(sigset_t *set) {
    __sets_errno(__syscall_sigpending(set));
}