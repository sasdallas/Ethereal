/**
 * @file hexahedron/task/syscalls/getsockopt.c
 * @brief getsockopt
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

long sys_getsockopt(int socket, int level, int option_name, void *option_value, socklen_t *option_len) {
    return socket_getsockopt(socket, level, option_name, option_value, option_len);
}
