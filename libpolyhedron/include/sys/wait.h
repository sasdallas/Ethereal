/**
 * @file libpolyhedron/include/sys/wait.h
 * @brief wait
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <sys/cheader.h>

_Begin_C_Header

#ifndef _SYS_WAIT_H
#define _SYS_WAIT_H

/**** INCLUDES ****/
#include <stdint.h>
#include <sys/types.h>

/**** DEFINITIONS ****/

#define WNOHANG             1
#define WUNTRACED           2
#define WEXITED             3
#define WSTOPPED            4
#define WCONTINUED          5
#define WNOWAIT             6

/**** TYPES ****/

typedef enum {
    P_ALL,
    P_PGID,
    P_PID,
} idtype_t;

/**** MACROS ****/

// returned from kernel:
//      bits 16-24: signal number of destroyed process
//      bits 8-16: exit status code of destroyed process
//      bit 3: process continued
//      bit 2: process was signalled
//      bit 1: process was stopped
//      bit 0: process exited
#define WSTATUS_SIGNUM      16
#define WSTATUS_EXITCODE    8
#define WSTATUS_CONTINUED   0x8
#define WSTATUS_SIGNALLED   0x4
#define WSTATUS_STOPPED     0x2
#define WSTATUS_EXITED      0x1

#define WEXITSTATUS(wstatus)    ((wstatus >> WSTATUS_EXITCODE) & 0xFF)
#define WIFCONTINUED(wstatus)   ((wstatus & WSTATUS_CONTINUED) == WSTATUS_CONTINUED)
#define WIFEXITED(wstatus)      ((wstatus & WSTATUS_EXITED) == WSTATUS_EXITED)
#define WIFSIGNALED(wstatus)    ((wstatus & WSTATUS_SIGNALLED) == WSTATUS_SIGNALLED)
#define WIFSTOPPED(wstatus)     ((wstatus & WSTATUS_STOPPED) == WSTATUS_STOPPED) // Not sure if this also needs to account for exited..
#define WSTOPSIG(wstatus)       ((wstatus >> WSTATUS_SIGNUM) & 0xFF)
#define WTERMSIG WSTOPSIG

/**** FUNCTIONS ****/

pid_t wait(int *wstatus);
pid_t waitpid(pid_t pid, int *wstatus, int options);

#endif

_End_C_Header