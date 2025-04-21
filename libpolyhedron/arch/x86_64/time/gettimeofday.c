/**
 * @file libpolyhedron/arch/x86_64/time/gettimeofday.c
 * @brief gettimeofday() function
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <time.h>
#include <sys/times.h>
#include <sys/syscall.h>
#include <unistd.h>

/* Call into the kernel if we're linked to it. Else use a system call */
#ifdef __LIBK

#include <kernel/drivers/clock.h>

int gettimeofday(struct timeval *p, void *z) {
    return clock_gettimeofday(p, z);
}

int settimeofday(struct timeval *p, void *z) {
    return clock_settimeofday(p, z);
}

#else

DEFINE_SYSCALL2(gettimeofday, SYS_GETTIMEOFDAY, struct timeval*, void*);

int gettimeofday(struct timeval *p, void *z) {
    __sets_errno(__syscall_gettimeofday(p, z));
}

#endif