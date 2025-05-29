/**
 * @file libpolyhedron/socket/send.c
 * @brief send, sendto, and sendmsg
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <errno.h>

DEFINE_SYSCALL3(sendmsg, SYS_SENDMSG, int, const struct msghdr*, int);

ssize_t sendmsg(int socket, const struct msghdr *message, int flags) {
    __sets_errno(__syscall_sendmsg(socket, message, flags));
}

ssize_t sendto(int socket, const void *message, size_t length, int flags, const struct sockaddr *dest_addr, socklen_t dest_len) {
    struct iovec iov = {
        .iov_base = (void*)message,
        .iov_len = length,
    };

    struct msghdr msg_hdr = {
        .msg_control = NULL,
        .msg_controllen = 0,
        .msg_iov = &iov,
        .msg_iovlen = 1,
        .msg_name = (void*)dest_addr,
        .msg_namelen = dest_len
    };

    return sendmsg(socket, &msg_hdr, flags);
}

ssize_t send(int socket, const void *buffer, size_t length, int flags) {
    struct iovec iov = {
        .iov_base = (void*)buffer,
        .iov_len = length
    };

    struct msghdr message = {
        .msg_control = NULL,
        .msg_controllen = 0,
        .msg_iov = &iov,
        .msg_iovlen = 1,
        .msg_name = NULL,
        .msg_namelen = 0
    };

    return sendmsg(socket, &message, flags);
}