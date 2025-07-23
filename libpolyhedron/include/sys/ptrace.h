/**
 * @file libpolyhedron/include/sys/ptrace.h
 * @brief ptrace
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <sys/cheader.h>

_Begin_C_Header

#ifndef _SYS_PTRACE_H
#define _SYS_PTRACE_H

/**** INCLUDES ****/
#include <sys/types.h>

/**** TYPES ****/

enum __ptrace_request {
    PTRACE_TRACEME,
    PTRACE_PEEKDATA,
    PTRACE_POKEDATA,
    PTRACE_GETREGS,
    PTRACE_GETFPREGS,
    PTRACE_GETREGSET,
    PTRACE_SETREGS,
    PTRACE_SETFPREGS,
    PTRACE_SETREGSET,
    PTRACE_GETSIGINFO,
    PTRACE_SETSIGINFO,
    PTRACE_PEEKSIGINFO,
    PTRACE_GETSIGMASK,
    PTRACE_SETSIGMASK,
    PTRACE_SETOPTIONS,
    PTRACE_GETEVENTMSG,
    PTRACE_CONT,
    PTRACE_SYSCALL,
    PTRACE_SINGLESTEP,
    PTRACE_SET_SYSCALL,
    PTRACE_LISTEN,
    PTRACE_KILL,
    PTRACE_INTERRUPT,
    PTRACE_ATTACH,
    PTRACE_SEIZE,
    PTRACE_DETACH,
    PTRACE_GET_SYSCALL_INFO,
};

enum __ptrace_event {
    PTRACE_EVENT_VFORK,
    PTRACE_EVENT_FORK,
    PTRACE_EVENT_CLONE,
    PTRACE_EVENT_EXEC,
    PTRACE_EVENT_EXIT,
    PTRACE_EVENT_STOP,
};

enum __ptrace_setoptions {
    PTRACE_O_TRACESYSGOOD           = 0x00000001,
    PTRACE_O_TRACEFORK              = 0x00000002,
    PTRACE_O_TRACEVFORK             = 0x00000004,
    PTRACE_O_TRACECLONE             = 0x00000008,
    PTRACE_O_TRACEEXEC              = 0x00000010,
    PTRACE_O_TRACEVFORKDONE         = 0x00000020,
    PTRACE_O_TRACEEXIT              = 0x00000040,
    PTRACE_O_MASK                   = 0x0000007F
};

/**** FUNCTIONS ****/
long ptrace(enum __ptrace_request op, pid_t pid, void *addr, void *data);

#endif

_End_C_Header