/**
 * @file libpolyhedron/unistd/times.c
 * @brief times
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <unistd.h>
#include <sys/syscall.h>
#include <errno.h>

DEFINE_SYSCALL1(times, SYS_TIMES, struct tms*);

clock_t times(struct tms *buf) {
    __sets_errno(__syscall_times(buf));
}