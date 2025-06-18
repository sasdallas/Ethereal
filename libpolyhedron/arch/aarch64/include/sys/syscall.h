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
#define SYSCALL_INSTRUCTION "svc 0"
#define SYSCALL_CLOBBERS    

/* System call definitions - NOT Linux compatible*/
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

/* Syscall macros */
#define DEFINE_SYSCALL0(name, num) \
    long __syscall_##name() { \
        register long __return_value __asm__("x0") = num;\
        asm volatile (SYSCALL_INSTRUCTION \
            : "=r"(__return_value) \
            : "r"(__return_value) : SYSCALL_CLOBBERS); \
        return __return_value;  \
    }

#define DEFINE_SYSCALL1(name, num, p1_type) \
    long __syscall_##name(p1_type p1) { \
        register long __return_value __asm__("x0") = num;\
        register long x1 __asm__("x1") = (long)p1;\
        asm volatile (SYSCALL_INSTRUCTION \
            : "=r"(__return_value) \
            : "r"(__return_value), "r"(x1) : SYSCALL_CLOBBERS); \
        return __return_value;  \
    }

#define DEFINE_SYSCALL2(name, num, p1_type, p2_type) \
    long __syscall_##name(p1_type p1, p2_type p2) { \
        register long __return_value __asm__("x0") = num;\
        register long x1 __asm__("x1") = (long)p1;\
        register long x2 __asm__("x2") = (long)p2;\
        asm volatile (SYSCALL_INSTRUCTION \
            : "=r"(__return_value) \
            : "r"(__return_value), "r"(x1), "r"(x2) : SYSCALL_CLOBBERS); \
        return __return_value;  \
    }

#define DEFINE_SYSCALL3(name, num, p1_type, p2_type, p3_type) \
    long __syscall_##name(p1_type p1, p2_type p2, p3_type p3) { \
        register long __return_value __asm__("x0") = num;\
        register long x1 __asm__("x1") = (long)p1;\
        register long x2 __asm__("x2") = (long)p2;\
        register long x3 __asm__("x3") = (long)p3;\
        asm volatile (SYSCALL_INSTRUCTION \
            : "=r"(__return_value) \
            : "r"(__return_value), "r"(x1), "r"(x2), "r"(x3) : SYSCALL_CLOBBERS); \
        return __return_value;  \
    }

#define DEFINE_SYSCALL4(name, num, p1_type, p2_type, p3_type, p4_type) \
    long __syscall_##name(p1_type p1, p2_type p2, p3_type p3, p4_type p4) { \
        register long __return_value __asm__("x0") = num;\
        register long x1 __asm__("x1") = (long)p1;\
        register long x2 __asm__("x2") = (long)p2;\
        register long x3 __asm__("x3") = (long)p3;\
        register long x4 __asm__("x4") = (long)p4;\
        asm volatile (SYSCALL_INSTRUCTION \
            : "=r"(__return_value) \
            : "r"(__return_value), "r"(x1), "r"(x2), "r"(x3), "r"(x4) : SYSCALL_CLOBBERS); \
        return __return_value;  \
    }

#define DEFINE_SYSCALL5(name, num, p1_type, p2_type, p3_type, p4_type, p5_type) \
    long __syscall_##name(p1_type p1, p2_type p2, p3_type p3, p4_type p4, p5_type p5) { \
        register long __return_value __asm__("x0") = num;\
        register long x1 __asm__("x1") = (long)p1;\
        register long x2 __asm__("x2") = (long)p2;\
        register long x3 __asm__("x3") = (long)p3;\
        register long x4 __asm__("x4") = (long)p4;\
        register long x5 __asm__("x5") = (long)p5;\
        asm volatile (SYSCALL_INSTRUCTION \
            : "=r"(__return_value) \
            : "r"(__return_value), "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5) : SYSCALL_CLOBBERS); \
        return __return_value;  \
    }

#endif

_End_C_Header