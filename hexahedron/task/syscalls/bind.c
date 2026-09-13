/**
 * @file hexahedron/task/syscalls/bind.c
 * @brief bind
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
#include <kernel/drivers/net/socket.h>

long sys_bind(int socket, const struct sockaddr *addr, socklen_t addrlen) {
    return socket_bind(socket, addr, addrlen);
}
