/**
 * @file libpolyhedron/signal/sigset.c
 * @brief Signal set functions
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

#define SIGNAL_VALIDATE(signum) if (signum < 0 || signum >= NUMSIGNALS) { errno = EINVAL; return -1; }

int sigaddset(sigset_t *set, int signum) {
    SIGNAL_VALIDATE(signum);
    *set |= (1 << signum);
    return 0;
}

int sigdelset(sigset_t *set, int signum) {
    SIGNAL_VALIDATE(signum);
    *set &= ~(1 << signum);
    return 0;
}

int sigemptyset(sigset_t *set) {
    *set = 0;
    return 0;
}

int sigfillset(sigset_t *set) {
    *set = (1ull << NUMSIGNALS) - 1;
    return 0;
}

int sigismember(const sigset_t *set, int signum) {
    SIGNAL_VALIDATE(signum);
    return (*set & (1 << signum));
}