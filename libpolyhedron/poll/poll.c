/**
 * @file libpolyhedron/poll/poll.c
 * @brief poll
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
#include <poll.h>
#include <errno.h>

DEFINE_SYSCALL3(poll, SYS_POLL, struct pollfd*, nfds_t, int);

int poll(struct pollfd fds[], nfds_t nfds, int timeout) {
    __sets_errno(__syscall_poll(fds, nfds, timeout));
}
