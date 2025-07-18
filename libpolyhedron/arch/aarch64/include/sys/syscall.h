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

#include <sys/syscall_nums.h>

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