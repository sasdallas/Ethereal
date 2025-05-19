/**
 * @file libpolyhedron/signal/signal.c
 * @brief signal
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <signal.h>
#include <errno.h>
#include <sys/signal.h>
#include <sys/syscall.h>
#include <sys/syscall_defs.h>

DEFINE_SYSCALL2(signal, SYS_SIGNAL, int, sighandler_t);

sighandler_t signal(int signum, sighandler_t handler) {
    long ret = __syscall_signal(signum, handler);
    if (ret < 0) {
        errno = -ret;
        return SIG_ERR;
    }

    return (sighandler_t)ret;
}