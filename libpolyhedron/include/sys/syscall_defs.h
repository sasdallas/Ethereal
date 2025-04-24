/**
 * @file libpolyhedron/include/sys/syscall_defs.h
 * @brief Contains system call functions
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

#ifndef _SYS_SYSCALL_DEFS_H
#define _SYS_SYSCALL_DEFS_H

/**** INCLUDES ****/
#include <sys/syscall.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stddef.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <dirent.h>

/**** MACROS ****/

#define DECLARE_SYSCALL0(name) long __syscall_##name()
#define DECLARE_SYSCALL1(name, p1_type) long __syscall_##name(p1_type p1)
#define DECLARE_SYSCALL2(name, p1_type, p2_type) long __syscall_##name(p1_type p1, p2_type p2)
#define DECLARE_SYSCALL3(name, p1_type, p2_type, p3_type) long __syscall_##name(p1_type p1, p2_type p2, p3_type p3)
#define DECLARE_SYSCALL4(name, p1_type, p2_type, p3_type, p4_type) long __syscall_##name(p1_type p1, p2_type p2, p3_type p3, p4_type p4)
#define DECLARE_SYSCALL5(name, p1_type, p2_type, p3_type, p4_type, p5_type) long __syscall_##name(p1_type p1, p2_type p2, p3_type p3, p4_type p4, p5_type p5)

#define __sets_errno(fn) {long _ret = fn; if (_ret < 0) { errno = -_ret; _ret = -1; } return _ret; }

/**** FUNCTIONS ****/

DECLARE_SYSCALL1(exit, int);
DECLARE_SYSCALL3(open, const char*, int, int);
DECLARE_SYSCALL3(read, int, void*, size_t);
DECLARE_SYSCALL3(write, int, const void*, size_t);
DECLARE_SYSCALL1(close, int);
DECLARE_SYSCALL2(stat, const char*, struct stat*);
DECLARE_SYSCALL2(fstat, int, struct stat*);
DECLARE_SYSCALL2(lstat, const char*, struct stat*);
DECLARE_SYSCALL3(ioctl, int, unsigned long, void*);
DECLARE_SYSCALL3(readdir, struct dirent*, int, unsigned long);
DECLARE_SYSCALL1(brk, void*);
DECLARE_SYSCALL0(fork);
DECLARE_SYSCALL3(lseek, int, off_t, int);
DECLARE_SYSCALL2(gettimeofday, struct timeval *, void*);
DECLARE_SYSCALL2(settimeofday, struct timeval *, void*);
DECLARE_SYSCALL1(usleep, useconds_t);
DECLARE_SYSCALL3(execve, const char*, const char **, char **);
DECLARE_SYSCALL3(wait, pid_t, int*, int);
DECLARE_SYSCALL2(getcwd, char*, size_t);
DECLARE_SYSCALL1(chdir, const char*);
DECLARE_SYSCALL1(fchdir, int);
DECLARE_SYSCALL1(uname, struct utsname*);
DECLARE_SYSCALL0(getpid);

#endif

_End_C_Header