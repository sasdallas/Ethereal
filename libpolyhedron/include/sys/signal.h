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

_Begin_C_Header

#ifndef _SYS_SIGNAL_H
#define _SYS_SIGNAL_H

/**** INCUDES ****/
#include <stdint.h>
#include <sys/types.h>

/**** DEFINITIONS ****/


/* Signal handler types */
#define SIG_DFL             ((void (*)(int))0)
#define SIG_IGN             ((void (*)(int))1)

/* Signal error */
#define SIG_ERR             ((void (*)(int))-1)

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
#define SIGWINCH            28  // (I) Window size changed

/* ETHEREAL CUSTOM SIGNALS */
#define SIGCANCEL           29  // (I) pthread cancel signal
#define SIGDISPLAY          30  // (I) Display size changed (TODO: maybe just a SIGWINCH?)

#define NUMSIGNALS          31
#define NSIG                NUMSIGNALS

/* sigprocmask */
#define SIG_BLOCK           0
#define SIG_UNBLOCK         1
#define SIG_SETMASK         2

/* sigaction flags */
#define SA_NOCLDSTOP        0x1     // Do not generate SIGCHLD when children stop
#define SA_ONSTACK          0x2     // Causes signal delivery to occur on another stack
#define SA_RESETHAND        0x4     // Causes signal dispositions to be set to SIG_DFL on entry to handlers
#define SA_RESTART          0x8     // Causes certain functions to become restartable
#define SA_SIGINFO          0x10    // Causes extra information to be passed to signal handlers at the time of receipt of a signal

/* SIGILL codes */
#define ILL_ILLOPC          1
#define ILL_ILLOPN          2
#define ILL_ILLADR          3
#define ILL_ILLTRP          4
#define ILL_PRVOPC          5
#define ILL_PRVREG          6
#define ILL_COPROC          7
#define ILL_BADSTK          8

/* SIGFPE codes */
#define FPE_INTDIV          1
#define FPE_INTOVF          2
#define FPE_FLTDIV          3
#define FPE_FLTOVF          4
#define FPE_FLTUND          5
#define FPE_FLTRES          6
#define FPE_FLTINV          7
#define FPE_FLTSUB          8

/* SEGV codes */
#define SEGV_MAPERR         1
#define SEGV_ACCERR         2

/* SIGBUS codes */
#define BUS_ADRALN          1
#define BUS_ADRERR          2
#define BUS_OBJERR          3

/* SIGTRAP codes */
#define TRAP_BRKPT          1
#define TRAP_TRACE          2

/* SIGCHLD codes */
#define CLD_EXITED          1
#define CLD_KILLED          2
#define CLD_DUMPED          3
#define CLD_TRAPPED         4
#define CLD_STOPPED         5
#define CLD_CONTINUED       6

/* SIGPOLL codes */
#define POLL_IN             1
#define POLL_OUT            2
#define POLL_MSG            3
#define POLL_ERR            4
#define POLL_PRI            5
#define POLL_HUP            6

/* Signal info */
#define SI_USER             9
#define SI_QUEUE            10
#define SI_TIMER            11
#define SI_ASYNCIO          12
#define SI_MESGQ            13


/**** TYPES ****/

typedef volatile int sig_atomic_t;
typedef unsigned long long sigset_t;

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
#ifdef __cplusplus

/* hack for C++... */

struct sigaction {
    union {
        void (*sa_handler)(int);
        void (*sa_sigaction)(int, siginfo_t*, void*);
    };

    sigset_t sa_mask;               // Set of signals to be blocked during execution
    int sa_flags;                   // Special flags
};

#else
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
#endif

typedef struct {
    void *ss_sp;                    // Stack base or pointer
    size_t ss_size;                 // Stack size
    int ss_flags;                   // Flags
} stack_t;

struct sigstack {
    int ss_onstack;                 // In-use
    void *ss_sp;                    // Signal stack pointer
};

/**** FUNCTIONS ****/

int    kill(pid_t, int);
int    killpg(pid_t, int);
int    pthread_kill(pthread_t, int);
int    pthread_sigmask(int, const sigset_t *, sigset_t *);
int    raise(int);
int    sigaction(int, const struct sigaction *, struct sigaction *);
int    sigaddset(sigset_t *, int);
int    sigaltstack(const stack_t *, stack_t *);
int    sigdelset(sigset_t *, int);
int    sigemptyset(sigset_t *);
int    sigfillset(sigset_t *);
int    siginterrupt(int, int);
int    sigismember(const sigset_t *, int);
void	(*signal(int sig, void (*func)(int)))(int);
int    sigpause(int);
int    sigpending(sigset_t *);
int    sigprocmask(int, const sigset_t *, sigset_t *);
int    sigqueue(pid_t, int, const union sigval);
int    sigsuspend(const sigset_t *);
int    sigwait(const sigset_t *set, int *sig);
int    sigwaitinfo(const sigset_t *, siginfo_t *);

#endif

_End_C_Header