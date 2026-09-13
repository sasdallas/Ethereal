/**
 * @file hexahedron/task/syscalls/socket.c
 * @brief socket
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

long sys_socket(int domain, int type, int protocol) {
    return socket_create(current_cpu->current_process, domain, type, protocol);
}
