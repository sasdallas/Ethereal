/**
 * @file hexahedron/task/syscalls/setsockopt.c
 * @brief setsockopt
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

long sys_setsockopt(sys_setopt_context_t *context) {
    // !!! context should be removed i am just lazy
    return socket_setsockopt(context->socket, context->level, context->option_name, context->option_value, context->option_len);
}
