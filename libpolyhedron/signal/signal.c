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
#include <stdio.h>


sighandler_t signal(int signum, sighandler_t handler) {
    printf("signal: %d %p\n", signum, handler);
    errno = EINVAL;
    return (sighandler_t)SIG_ERR;
}