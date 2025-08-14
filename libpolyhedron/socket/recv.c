/**
 * @file libpolyhedron/socket/recv.c
 * @brief recv, recvmsg, and recvfrom
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 * 
 * @note Usage of syscalls sendmsg and recvmsg are inspired by ToaruOS
 */

#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <errno.h>

DEFINE_SYSCALL3(recvmsg, SYS_RECVMSG, int, struct msghdr*, int);

ssize_t recvmsg(int socket, struct msghdr *message, int flags) {
    __sets_errno(__syscall_recvmsg(socket, message, flags));
}

ssize_t recvfrom(int socket, void *buffer, size_t len, int flags, struct sockaddr *address, socklen_t *address_len) {
    // Handle
    struct iovec iov = {
        .iov_base = buffer,
        .iov_len = len
    };

    struct msghdr message = {
        .msg_control = NULL,
        .msg_controllen = 0,
        .msg_flags = 0,
        .msg_iov = &iov,
        .msg_iovlen = 1,
        .msg_name = address,
        .msg_namelen = address_len ? *address_len : 0
    };

    ssize_t result = recvmsg(socket, &message, flags);
    if (address_len) *address_len = message.msg_namelen;    // Update result
    return result;
}

ssize_t recv(int socket, void *buffer, size_t length, int flags) {
    struct iovec iov = {
        .iov_base = buffer,
        .iov_len = length
    };

    struct msghdr message = {
        .msg_control = NULL,
        .msg_controllen = 0,
        .msg_flags = 0,
        .msg_iov = &iov,
        .msg_iovlen = 1,
        .msg_name = NULL,
        .msg_namelen = 0
    };

    return recvmsg(socket, &message, flags);
}