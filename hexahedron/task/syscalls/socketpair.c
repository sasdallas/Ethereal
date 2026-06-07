/**
 * @file hexahedron/task/syscalls/socketpair.c
 * @brief socketpair
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/task/process.h>
#include <kernel/drivers/net/socket.h>
#include <sys/socket.h>

long sys_socketpair(int domain, int type, int protocol, int socket_vector[2]) {
    SYSCALL_VALIDATE_PTR(socket_vector);
    return socket_pair(domain, type, protocol, socket_vector);
}
