/**
 * @file hexahedron/task/syscalls/poll.c
 * @brief poll
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#include <kernel/task/process.h>

long sys_poll(struct pollfd fds[], nfds_t nfds, int timeout) {
    if (!nfds) return 0;
    SYSCALL_VALIDATE_PTR_SIZE(fds, sizeof(struct pollfd) * nfds);

    int have_hit = 0;

    poll_waiter_t *waiter = poll_createWaiter(current_cpu->current_thread, nfds ? nfds : 1);

    for (size_t i = 0; i < nfds; i++) {
        // Check the file descriptor
        fds[i].revents = 0;
        if (!FD_VALIDATE(fds[i].fd)) {
            fds[i].revents |= POLLNVAL;
            continue;
        }

        // Does the file descriptor have available contents right now?
        poll_events_t revents = 0;
        int ready = vfs_poll(FD(fds[i].fd), waiter, fds[i].events, &revents);
        fds[i].revents = (short)revents;

        if (ready == 1) {
            // Oh, we already have a hit! :D
            have_hit++;
        }
    }

    if (have_hit) {
        poll_exit(waiter);
        poll_destroyWaiter(waiter);
        return have_hit;
    }

    // We didn't get anything. Did they want us to wait?
    if (timeout == 0) {
        poll_exit(waiter);
        poll_destroyWaiter(waiter);
        return 0;
    }
    
    // Yes, so prepare ourselves to wait
    int w = poll_wait(waiter, timeout);


    if (w == -EINTR) {
        poll_exit(waiter);
        poll_destroyWaiter(waiter);
        return -EINTR;
    }

    if (w == -ETIMEDOUT) {
        poll_exit(waiter);
        poll_destroyWaiter(waiter);
        return 0;
    }

    for (size_t i = 0; i < nfds; i++) {
        // Does the file descriptor have available contents right now?
        if (!FD_VALIDATE(fds[i].fd)) continue;
        
        // !!!: FLOW BREAKING TRASH
        vfs_file_t *f = FD(fds[i].fd);
        assert(f->ops && f->ops->poll_events);
        fds[i].revents = (short)(f->ops->poll_events(f) & (fds[i].events | POLLHUP | POLLERR));
        if (fds[i].revents) {
            have_hit++;
        }
    }

    poll_exit(waiter);
    poll_destroyWaiter(waiter);
    return have_hit;   // At least one thread woke us up
}
