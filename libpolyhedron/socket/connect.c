/**
 * @file libpolyhedron/socket/connect.c
 * @brief connect
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
#include <errno.h>

DEFINE_SYSCALL3(connect, SYS_CONNECT, int, const struct sockaddr*, socklen_t);

int connect(int socket, const struct sockaddr *addr, socklen_t addrlen) {
    __sets_errno(__syscall_connect(socket, addr, addrlen));
}