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
#define SIG_DFL             (sa_handler)0
#define SIG_IGN             (sa_handler)1

/* Signal handler return */
#define SIG_ERR             (sa_handler)-1

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

#define NUMSIGNALS          28

/* sigaction flags */
#define SA_NOCLDSTOP        0x1     // Do not generate SIGCHLD when children stop
#define SA_ONSTACK          0x2     // Causes signal delivery to occur on another stack
#define SA_RESETHAND        0x4     // Causes signal dispositions to be set to SIG_DFL on entry to handlers
#define SA_RESTART          0x8     // Causes certain functions to become restartable
#define SA_SIGINFO          0x10    // Causes extra information to be passed to signal handlers at the time of receipt of a signal



/**** TYPES ****/

typedef volatile int sig_atomic_t;
typedef unsigned long sigset_t;

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


typedef void (*sighandler_t)(int);

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
int kill(pid_t pid, int sig);
int sigaction(int sig, const struct sigaction *act, struct sigaction *oact);

#endif

_End_C_Header;