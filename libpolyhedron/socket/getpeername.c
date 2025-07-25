/**
 * @file libpolyhedron/socket/getpeername.c
 * @brief getpeername
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

DEFINE_SYSCALL3(getpeername, SYS_GETPEERNAME, int, struct sockaddr*, socklen_t*);

int getpeername(int socket, struct sockaddr *address, socklen_t *address_len) {
    __sets_errno(__syscall_getpeername(socket, address, address_len));
}