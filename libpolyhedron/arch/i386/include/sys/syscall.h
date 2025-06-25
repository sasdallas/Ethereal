/**
 * @file libpolyhedron/include/sys/syscall.h
 * @brief System call
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

#ifndef _SYS_SYSCALL_H
#define _SYS_SYSCALL_H

/* System call instruction */
#define SYSCALL_INSTRUCTION "int $128"
#define SYSCALL_CLOBBERS    "memory"

/* System call definitions */
#define SYS_EXIT            0
#define SYS_OPEN            2
#define SYS_READ            3
#define SYS_WRITE           4
#define SYS_CLOSE           5
#define SYS_STAT            6
#define SYS_FSTAT           7
#define SYS_LSTAT           8
#define SYS_IOCTL           9
#define SYS_READDIR         10
#define SYS_POLL            11
#define SYS_MKDIR           12
/* reserved */
#define SYS_BRK             20
#define SYS_FORK            21
#define SYS_LSEEK           22
#define SYS_GETTIMEOFDAY    23
#define SYS_SETTIMEOFDAY    24
#define SYS_USLEEP          25
#define SYS_EXECVE          26
#define SYS_WAIT            27
#define SYS_GETCWD          28
#define SYS_CHDIR           29
#define SYS_FCHDIR          30
#define SYS_UNAME           31
#define SYS_GETPID          32
#define SYS_TIMES           33
#define SYS_MMAP            34
#define SYS_MPROTECT        35
#define SYS_MUNMAP          36
#define SYS_MSYNC           37
#define SYS_DUP2            38
#define SYS_SIGNAL          39
#define SYS_SIGACTION       40
#define SYS_SIGPENDING      41
#define SYS_SIGPROCMASK     42
#define SYS_SIGSUSPEND      43
#define SYS_SIGWAIT         44
#define SYS_KILL            45
#define SYS_SOCKET          46
#define SYS_BIND            47
#define SYS_ACCEPT          48
#define SYS_LISTEN          49
#define SYS_CONNECT         50
#define SYS_GETSOCKOPT      51
#define SYS_SETSOCKOPT      52
#define SYS_SENDMSG         53
#define SYS_RECVMSG         54
#define SYS_SHUTDOWN        55
#define SYS_GETSOCKNAME     56
#define SYS_GETPEERNAME     57
#define SYS_SOCKETPAIR      58
#define SYS_MOUNT           59
#define SYS_UMOUNT          60
#define SYS_PIPE            61
#define SYS_SHARED_NEW      62  // Ethereal API
#define SYS_SHARED_KEY      63  // Ethereal API
#define SYS_SHARED_OPEN     64  // Ethereal API
#define SYS_CREATE_THREAD   65  // Ethereal API (pthread)
#define SYS_GETTID          66  // Ethereal API (pthread)
#define SYS_SETTLS          67  // Ethereal API (pthread)
#define SYS_EXIT_THREAD     68  // Ethereal API (pthread)
#define SYS_JOIN_THREAD     69  // Ethereal API (pthread)
#define SYS_KILL_THREAD     70  // Ethereal API (pthread)

/* Syscall macros */
#define DEFINE_SYSCALL0(name, num) \
    long __syscall_##name() { \
        long __return_value = num;\
        asm volatile (SYSCALL_INSTRUCTION \
            : "=a"(__return_value) \
            : "a"(__return_value) : SYSCALL_CLOBBERS); \
        return __return_value;  \
    }

#define DEFINE_SYSCALL1(name, num, p1_type) \
    long __syscall_##name(p1_type p1) { \
        long __return_value = num;\
        asm volatile (SYSCALL_INSTRUCTION \
            : "=a"(__return_value) \
            : "a"(__return_value), "b"((long)(p1)) : SYSCALL_CLOBBERS); \
        return __return_value;  \
    }

#define DEFINE_SYSCALL2(name, num, p1_type, p2_type) \
    long __syscall_##name(p1_type p1, p2_type p2) { \
        long __return_value = num;\
        asm volatile (SYSCALL_INSTRUCTION \
            : "=a"(__return_value) \
            : "a"(__return_value), "b"((long)(p1)), "c"((long)(p2)) : SYSCALL_CLOBBERS); \
        return __return_value;  \
    }

#define DEFINE_SYSCALL3(name, num, p1_type, p2_type, p3_type) \
    long __syscall_##name(p1_type p1, p2_type p2, p3_type p3) { \
        long __return_value = num;\
        asm volatile (SYSCALL_INSTRUCTION \
            : "=a"(__return_value) \
            : "a"(__return_value), "b"((long)(p1)), "c"((long)(p2)), "d"((long)(p3)) : SYSCALL_CLOBBERS); \
        return __return_value;  \
    }

#define DEFINE_SYSCALL4(name, num, p1_type, p2_type, p3_type, p4_type) \
    long __syscall_##name(p1_type p1, p2_type p2, p3_type p3, p4_type p4) { \
        long __return_value = num;\
        asm volatile (SYSCALL_INSTRUCTION \
            : "=a"(__return_value) \
            : "a"(__return_value), "b"((long)(p1)), "c"((long)(p2)), "d"((long)(p3)), "S"((long)(p4)) : SYSCALL_CLOBBERS); \
        return __return_value;  \
    }

#define DEFINE_SYSCALL5(name, num, p1_type, p2_type, p3_type, p4_type, p5_type) \
    long __syscall_##name(p1_type p1, p2_type p2, p3_type p3, p4_type p4, p5_type p5) { \
        long __return_value = num;\
        asm volatile (SYSCALL_INSTRUCTION \
            : "=a"(__return_value) \
            : "a"(__return_value), "b"((long)(p1)), "c"((long)(p2)), "d"((long)(p3)), "S"((long)(p4)), "D"((long)(p5)) : SYSCALL_CLOBBERS); \
        return __return_value;  \
    }


#endif

_End_C_Header