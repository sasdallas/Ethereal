/**
 * @file libpolyhedron/socket/socket.c
 * @brief socket and socketpair
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

DEFINE_SYSCALL4(socketpair, SYS_SOCKETPAIR, int, int, int, int*);
DEFINE_SYSCALL3(socket, SYS_SOCKET, int, int, int);

int socketpair(int domain, int type, int protocol, int socket_vector[2]) {
    __sets_errno(__syscall_socketpair(domain, type, protocol, socket_vector));
}

int socket(int domain, int type, int protocol) {
    __sets_errno(__syscall_socket(domain, type, protocol));
}