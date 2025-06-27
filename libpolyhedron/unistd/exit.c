/**
 * @file libpolyhedron/unistd/exit.c
 * @brief exit
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <unistd.h>
#include <sys/syscall.h>

DEFINE_SYSCALL1(exit, SYS_EXIT, int);

void __attribute__((noreturn)) _exit(int status) {
    __syscall_exit(status);
    __builtin_unreachable();
}

void __attribute__((noreturn)) exit(int status) {
    _exit(status);
}