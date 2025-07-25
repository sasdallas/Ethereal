/**
 * @file libpolyhedron/socket/getsockopt.c
 * @brief getsockopt
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

DEFINE_SYSCALL5(getsockopt, SYS_GETSOCKOPT, int, int, int, void*, socklen_t*);

int getsockopt(int socket, int level, int option_name, void *option, socklen_t *option_len) {
    __sets_errno(__syscall_getsockopt(socket, level, option_name, option, option_len));
}