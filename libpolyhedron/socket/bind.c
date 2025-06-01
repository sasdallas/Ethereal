/**
 * @file libpolyhedron/socket/bind.c
 * @brief bind
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

DEFINE_SYSCALL3(bind, SYS_BIND, int, const struct sockaddr*, socklen_t);

int bind(int socket, const struct sockaddr *addr, socklen_t len) {
    __sets_errno(__syscall_bind(socket, addr, len));
}