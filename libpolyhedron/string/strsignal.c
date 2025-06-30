/**
 * @file libpolyhedron/signal/strsignal.c
 * @brief strsignal
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <sys/signal.h>
#include <string.h>
#include <stdio.h>

static const char *sys_siglist[] = {
    [SIGABRT]           = "Aborted",
    [SIGALRM]           = "Alarm clock",
    [SIGBUS]            = "Bus error",
    [SIGCHLD]           = "Child process state change",
    [SIGFPE]            = "Arithmetic exception",
    [SIGHUP]            = "Hang up",
    [SIGILL]            = "Illegal instruction",
    [SIGINT]            = "Interrupt",
    [SIGKILL]           = "Killed",
    [SIGPIPE]           = "Broken pipe",
    [SIGQUIT]           = "Quit",
    [SIGSEGV]           = "Segmentation fault",
    [SIGSTOP]           = "Stopped",
    [SIGTERM]           = "Terminated",
    [SIGTSTP]           = "Stopped",
    [SIGTTIN]           = "Stopped (tty input)",
    [SIGTTOU]           = "Stopped (tty output)",
    [SIGUSR1]           = "User-defined signal 1",
    [SIGUSR2]           = "User-defined signal 2",
    [SIGPOLL]           = "Pollable event",
    [SIGPROF]           = "Profiling timer expired",
    [SIGSYS]            = "Bad system call",
    [SIGTRAP]           = "Trace/breakpoint trap",
    [SIGURG]            = "High bandwidth data available",
    [SIGVTALRM]         = "Virtual timer expired",
    [SIGXCPU]           = "CPU time limit exceeded",
    [SIGXFSZ]           = "File size limit exceeded",
    [SIGWINCH]          = "Window size changed",
};

/* Signal description */
static char __sigstr[256];

char *strsignal(int sig) {
    if (sig < 0 || sig >= NUMSIGNALS) {
        snprintf(__sigstr, 256, "Unknown signal %d", sig);
    } else {
        snprintf(__sigstr, 256, "%s", sys_siglist[sig]);
    }

    return __sigstr;
}