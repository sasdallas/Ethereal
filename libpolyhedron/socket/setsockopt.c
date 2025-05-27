/**
 * @file libpolyhedron/socket/setsockopt.c
 * @brief setsockopt
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

// !!!: I do not know why this is required
struct sockopt_params {
    int socket;
    int level;
    int option_name;
    const void *option_value;
    socklen_t option_len;
};

DEFINE_SYSCALL1(setsockopt, SYS_SETSOCKOPT, struct sockopt_params*);

int setsockopt(int socket, int level, int option_name, const void *option_value, socklen_t option_len) {
    struct sockopt_params params = {
        .socket = socket,
        .level = level,
        .option_name = option_name,
        .option_value = option_value,
        .option_len = option_len
    };

    __sets_errno(__syscall_setsockopt(&params));
}