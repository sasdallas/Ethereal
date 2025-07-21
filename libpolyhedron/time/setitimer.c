/**
 * @file libpolyhedron/time/setitimer.c
 * @brief setitimer
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef __LIBK
#include <sys/time.h>
#include <sys/syscall.h>
#include <errno.h>

DEFINE_SYSCALL3(setitimer, SYS_SETITIMER, int, const struct itimerval*, struct itimerval *);

int setitimer(int which, const struct itimerval *value, struct itimerval *ovalue) {
    __sets_errno(__syscall_setitimer(which, value, ovalue));
}

#endif