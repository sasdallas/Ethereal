/**
 * @file libpolyhedron/socket/accept.c
 * @brief accept
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

DEFINE_SYSCALL3(accept, SYS_ACCEPT, int, struct sockaddr*, socklen_t *)

int accept(int socket, struct sockaddr *addr, socklen_t *addrlen) {
    __sets_errno(__syscall_accept(socket, addr, addrlen));
}