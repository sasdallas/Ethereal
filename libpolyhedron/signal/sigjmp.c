/**
 * @file libpolyhedron/signal/sigjmp.c
 * @brief sigsetjmp and siglongjmp
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <setjmp.h>
#include <signal.h>

void siglongjmp(sigjmp_buf buf, int val) {
    if (buf[_JBLEN]) {
        // We have a sigmask
        sigprocmask(SIG_SETMASK, (const sigset_t*)&buf[_JBLEN + 1], NULL);
    }

    longjmp(buf, val);
}

int sigsetjmp(sigjmp_buf buf, int save) {
    buf[_JBLEN] = save;

    if (save) {
        // Save the sigmask
        sigprocmask(0, NULL, (sigset_t*)&buf[_JBLEN+1]);
    }

    return setjmp(buf);
}