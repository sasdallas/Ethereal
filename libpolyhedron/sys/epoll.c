/**
 * @file libpolyhedron/sys/epoll.c
 * @brief epoll
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <sys/epoll.h>
#include <sys/syscall.h>
#include <errno.h>

DEFINE_SYSCALL1(epoll_create, SYS_EPOLL_CREATE, int);
DEFINE_SYSCALL5(epoll_pwait, SYS_EPOLL_PWAIT, int, struct epoll_event*, int, int, const sigset_t*);
DEFINE_SYSCALL4(epoll_ctl, SYS_EPOLL_CTL, int, int, int, const struct epoll_event*);

int epoll_create(int size) {
    if (size <= 0) {
        errno = EINVAL;
        return -1;
    }

    __sets_errno(__syscall_epoll_create(size));
}

int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event) {
    __sets_errno(__syscall_epoll_ctl(epfd, op, fd, event));
}

int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout) {
    return epoll_pwait(epfd, events, maxevents, timeout, NULL);
}

int epoll_pwait(int epfd, struct epoll_event *events, int maxevents, int timeout, const sigset_t *sigmask) {
    __sets_errno(__syscall_epoll_pwait(epfd, events, maxevents, timeout, sigmask));
}