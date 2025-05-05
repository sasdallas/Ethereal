/**
 * @file libpolyhedron/include/signal.h
 * @brief signal
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

 #include <sys/cheader.h>

_Begin_C_Header;

#ifndef _SYS_SIGNAL_H
#define _SYS_SIGNAL_H

/**** INCUDES ****/
#include <stdint.h>
#include <sys/types.h>

/**** DEFINITIONS ****/

/* Signal handling types */
#define SIG_DFL             0
#define SIG_ERR             1
#define SIG_HOLD            2
#define SIG_IGN             3

/* sigev_notify values */
#define SIGEV_NONE          0
#define SIGEV_SIGNAL        1
#define SIGEV_THREAD        2

/* Signals */
#define SIGABRT             0   // (A) Process abort signal
#define SIGALRM             1   // (T) Alarm clock
#define SIGBUS              2   // (A) Access to undefined portion of memory object
#define SIGCHLD             3   // (I) Child process state change
#define SIGCONT             4   // (C) Continue execution
#define SIGFPE              5   // (A) Erroneous arithmetic operation
#define SIGHUP              6   // (T) Hang up
#define SIGILL              7   // (A) Illegal instruction
#define SIGINT              8   // (T) Terminal interrupt signal
#define SIGKILL             9   // (T) Kill (cannot be caught or ignored)
#define SIGPIPE             10  // (T) Write to a pipe with no readers
#define SIGQUIT             11  // (A) Terminal quit signal
#define SIGSEGV             12  // (A) Invalid memory reference (my favorite!)
#define SIGSTOP             13  // (S) Stop executing (cannot be caught or ignored)
#define SIGTERM             14  // (T) Termination signal
#define SIGTSTP             15  // (S) Terminal stop signal
#define SIGTTIN             16  // (S) Background process attempting read
#define SIGTTOU             17  // (S) Background process attempting write
#define SIGUSR1             18  // (T) User-defined signal 1
#define SIGUSR2             19  // (T) User-defined signal 2
#define SIGPOLL             20  // (T) Pollable event
#define SIGPROF             21  // (T) Profiling timer expired
#define SIGSYS              22  // (A) Bad system call
#define SIGTRAP             23  // (A) Trace/breakpoint trap
#define SIGURG              24  // (I) High bandwidth data available
#define SIGVTALRM           25  // (T) Virtual timer expired
#define SIGXCPU             26  // (A) CPU time limit exceeded
#define SIGXFSZ             27  // (A) File size limit exceeded

/**** TYPES ****/

typedef volatile int sig_atomic_t;
typedef int sigset_t;

union sigval {
    int     sival_int;
    void    *sival_ptr;
};

typedef struct {
    int si_signo;           // Signal number
    int si_code;            // Signal code
    int si_errno;           // errno value associated with signal
    pid_t si_pid;           // Sending process ID
    uid_t si_uid;           // User ID of sending process
    void *si_addr;          // Address of faulting instruction
    int si_status;          // Exit value or signal
    long si_band;           // Band event for SIGPOLL
    union sigval si_value;  // Signal value
} siginfo_t;


typedef typeof(void (int))  *sighandler_t;

// Signal-catching functions
typedef void (*sa_handler)(int);
typedef void (*sa_sigaction)(int, siginfo_t*, void*);

struct sigaction {
    union {
        sa_handler sa_handler;          // Signal-catching function or macro SIG_IGN / SIG_DFL
        sa_sigaction sa_sigaction;      // Signal-catching function
    };

    sigset_t sa_mask;               // Set of signals to be blocked during execution
    int sa_flags;                   // Special flags
};

/**** FUNCTIONS ****/

sighandler_t signal(int signum, sighandler_t handler);
int raise(int sig);

#endif

_End_C_Header;