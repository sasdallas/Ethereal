/**
 * @file hexahedron/task/syscalls/pselect.c
 * @brief pselect
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


long sys_pselect(sys_pselect_context_t *ctx) {
    if (ctx->readfds) SYSCALL_VALIDATE_PTR(ctx->readfds);
    if (ctx->writefds) SYSCALL_VALIDATE_PTR(ctx->writefds);
    if (ctx->errorfds) SYSCALL_VALIDATE_PTR(ctx->errorfds);
    if (ctx->timeout) SYSCALL_VALIDATE_PTR(ctx->timeout);
    if (ctx->sigmask) SYSCALL_VALIDATE_PTR(ctx->sigmask);

    sigset_t old_set = current_cpu->current_thread->blocked_signals;

    if (ctx->sigmask) {
        current_cpu->current_thread->blocked_signals = *(ctx->sigmask);
    }

    // Create the return sets
    fd_set rfds, wfds, efds;
    FD_ZERO(&rfds);
    FD_ZERO(&wfds);
    FD_ZERO(&efds);

    size_t ret = 0;

    poll_waiter_t *waiter = poll_createWaiter(current_cpu->current_thread, ctx->nfds);

    for (int fd = 0; fd < ctx->nfds; fd++) {
        if (!((ctx->readfds && FD_ISSET(fd, ctx->readfds)) || (ctx->writefds && FD_ISSET(fd, ctx->writefds)) || (ctx->errorfds && FD_ISSET(fd, ctx->errorfds)))) {
            continue; // No need to care, it isn't a wanted file descriptor
        }

        // Validate file descriptor
        if (!FD_VALIDATE(fd)) continue;

        poll_events_t needed = 0;
        if (ctx->readfds && FD_ISSET(fd, ctx->readfds)) needed |= POLLIN;
        if (ctx->writefds && FD_ISSET(fd, ctx->writefds)) needed |= POLLOUT;
        if (ctx->errorfds && FD_ISSET(fd, ctx->errorfds)) needed |= POLLERR;

        poll_events_t out = 0;
        int ready = vfs_poll(FD(fd), waiter, needed, &out);
        if (ready == 1) {
            // Wow we have a hit oh my goodness
            if (out & POLLIN) FD_SET(fd, &rfds);
            if (out & POLLOUT) FD_SET(fd, &wfds);
            if (out & POLLERR) FD_SET(fd, &efds);
            ret++;
        }
    }


    int timeout = -1;
    if (ctx->timeout) timeout = (ctx->timeout->tv_sec * 1000) + (ctx->timeout->tv_nsec / 1000);
    
    if (ret || timeout == 0) {
        (current_cpu->current_thread->blocked_signals) = old_set;
        poll_exit(waiter);
        poll_destroyWaiter(waiter);
        if (ctx->readfds) memcpy(ctx->readfds, &rfds, sizeof(fd_set));
        if (ctx->writefds) memcpy(ctx->writefds, &wfds, sizeof(fd_set));
        if (ctx->errorfds) memcpy(ctx->errorfds, &efds, sizeof(fd_set));
        return ret;
    }

    int w = poll_wait(waiter, timeout);

    if (w == -EINTR) {
        (current_cpu->current_thread->blocked_signals) = old_set;
        poll_exit(waiter);
        poll_destroyWaiter(waiter);
        return -EINTR;
    }

    if (w == -ETIMEDOUT) {
        poll_exit(waiter);
        poll_destroyWaiter(waiter);
    }

    for (int fd = 0; fd < ctx->nfds; fd++) {
        if (!((ctx->readfds && FD_ISSET(fd, ctx->readfds)) || (ctx->writefds && FD_ISSET(fd, ctx->writefds)) || (ctx->errorfds && FD_ISSET(fd, ctx->errorfds)))) {
            continue; // No need to care, it isn't a wanted file descriptor
        }

        // Does the file descriptor have available contents right now?
        if (!FD_VALIDATE(fd)) continue;
        
        poll_events_t needed = 0;
        if (ctx->readfds && FD_ISSET(fd, ctx->readfds)) needed |= POLLIN;
        if (ctx->writefds && FD_ISSET(fd, ctx->writefds)) needed |= POLLOUT;
        if (ctx->errorfds && FD_ISSET(fd, ctx->errorfds)) needed |= POLLERR;

        // !!!: FLOW BREAKING TRASH
        vfs_file_t *f = FD(fd);
        assert(f->ops && f->ops->poll_events);
        poll_events_t out = f->ops->poll_events(f) & needed;
        if (out) {
            ret++;

            if (out & POLLIN) FD_SET(fd, &rfds);
            if (out & POLLOUT) FD_SET(fd, &wfds);
            if (out & POLLERR) FD_SET(fd, &efds);
        }        
    }



    if (ctx->readfds) memcpy(ctx->readfds, &rfds, sizeof(fd_set));
    if (ctx->writefds) memcpy(ctx->writefds, &wfds, sizeof(fd_set));
    if (ctx->errorfds) memcpy(ctx->errorfds, &efds, sizeof(fd_set));

    (current_cpu->current_thread->blocked_signals) = old_set;

    if (w != -ETIMEDOUT) {
        poll_exit(waiter);
        poll_destroyWaiter(waiter);
    }

    return ret;
}
