/**
 * @file hexahedron/task/syscalls/pause.c
 * @brief pause
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

long sys_pause() {
    sleep_prepare();
    sleep_enter();
    return -EINTR;
}
