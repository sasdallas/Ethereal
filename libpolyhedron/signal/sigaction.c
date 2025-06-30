/**
 * @file libpolyhedron/signal/sigaction.c
 * @brief sigaction
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <sys/signal.h>
#include <errno.h>
#include <sys/syscall.h>

DEFINE_SYSCALL3(sigaction, SYS_SIGACTION, int, const struct sigaction*, struct sigaction*);

int sigaction(int sig, const struct sigaction *act, struct sigaction *oact) {
    __sets_errno(__syscall_sigaction(sig, act, oact));
}