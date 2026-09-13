/**
 * @file hexahedron/task/syscalls/pipe.c
 * @brief pipe
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
#include <kernel/fs/pipe.h>

long sys_pipe(int fildes[2]) {
    return pipe_create(fildes);
}
