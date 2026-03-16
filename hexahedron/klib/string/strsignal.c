/**
 * @file hexahedron/klib/signal/strsignal.c
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

static const char *__siglist[] = {
    [SIGHUP]            = "Hang up",
    [SIGINT]            = "Interrupt",
    [SIGQUIT]           = "Quit",
    [SIGILL]            = "Illegal instruction",
    [SIGTRAP]           = "Trace/breakpoint trap",
    [SIGABRT]           = "Aborted",
    [SIGBUS]            = "Bus error",
    [SIGFPE]            = "Arithmetic exception",
    [SIGKILL]           = "Killed",
    [SIGUSR1]           = "User-defined signal 1",
    [SIGSEGV]           = "Segmentation fault",
    [SIGUSR2]           = "User-defined signal 2",
    [SIGPIPE]           = "Broken pipe",
    [SIGALRM]           = "Alarm clock",
    [SIGTERM]           = "Terminated",
    [SIGSTKFLT]         = "Stack fault on coprocessor",
    [SIGCHLD]           = "Child process state change",
    [SIGCONT]           = "Continued",
    [SIGSTOP]           = "Stopped",
    [SIGTSTP]           = "Stopped",
    [SIGTTIN]           = "Stopped (tty input)",
    [SIGTTOU]           = "Stopped (tty output)",
    [SIGURG]            = "High bandwidth data available",
    [SIGXCPU]           = "CPU time limit exceeded",
    [SIGXFSZ]           = "File size limit exceeded",
    [SIGVTALRM]         = "Virtual timer expired",
    [SIGPROF]           = "Profiling timer expired",
    [SIGWINCH]          = "Window size changed",
    [SIGPOLL]           = "Pollable event",
    [SIGPWR]            = "SIGPWR", // i dont actually know what this is for
    [SIGSYS]            = "Bad system call",
    [SIGCANCEL]         = "Thread cancelled",
    [NSIG]              = "Unknown signal",
};

char *strsignal(int sig) {
    if (sig > NSIG || sig < 0) sig = NSIG;
    return (char*)__siglist[sig];
}