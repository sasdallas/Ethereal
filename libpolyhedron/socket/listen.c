/**
 * @file libpolyhedron/socket/listen.c
 * @brief listen
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

DEFINE_SYSCALL2(listen, SYS_LISTEN, int, int);

int listen(int socket, int backlog) {
    __sets_errno(__syscall_listen(socket, backlog));
}