/**
 * @file hexahedron/task/syscalls/sendmsg.c
 * @brief sendmsg
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

long sys_sendmsg(int socket, struct msghdr *message, int flags) {
    if (flags) SYSCALL_LOG(WARN, "sys_sendmsg: flags are 0x%x\n", flags);
    return socket_sendmsg(socket, message, flags);
}
