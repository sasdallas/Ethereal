/**
 * @file libpolyhedron/poll/select.c
 * @brief select
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <sys/select.h>
#include <sys/syscall.h>
#include <errno.h>

struct pselect_ctx {
    int nfds;
    fd_set *readfds;
    fd_set *writefds;
    fd_set *errorfds;
    const struct timespec *timeout;
    const sigset_t *sigmask;
};

DEFINE_SYSCALL1(pselect, SYS_PSELECT, struct pselect_ctx*);

int pselect(int nfds, fd_set *readfds, fd_set *writefds, fd_set *errorfds, const struct timespec *timeout, const sigset_t *sigmask) {
    struct pselect_ctx ctx = {
        .nfds = nfds,
        .readfds = readfds,
        .writefds = writefds,
        .errorfds = errorfds,
        .timeout = timeout,
        .sigmask = sigmask,
    };

    __sets_errno(__syscall_pselect(&ctx));
}

int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *errorfds, struct timeval *timeout) {
    struct timespec *t = NULL;
    struct timespec tm;

    if (timeout) {
        tm.tv_sec = timeout->tv_sec;
        tm.tv_nsec = timeout->tv_usec * 1000;
        t = &tm;
    }

    return pselect(nfds, readfds, writefds, errorfds, (const struct timespec*)t, NULL);
}