/**
 * @file libpolyhedron/unistd/getpid.c
 * @brief getpid syscall
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 * 
 * @author Rubyboat
 */

#include <unistd.h>

DEFINE_SYSCALL0(getpid, SYS_GETPID);

pid_t getpid() {
    return __syscall_getpid();
}