/**
 * @file userspace/lib/include/ethereal/user.h
 * @brief user registers
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _USER_H
#define _USER_H

/* !!!: BUILD HACK */
#ifdef _SYS_USER_H
#error "You must not include sys/user.h"
#endif

/**** INCLUDES ****/
#include <stdint.h>

/**** TYPES ****/

#if defined(__i686__) || defined(__i386__)

struct user_regs_struct {
    uint64_t eax;
    uint64_t ebx;
    uint64_t ecx;
    uint64_t edx;
    uint64_t esi;
    uint64_t edi;
    uint64_t ebp;
    uint64_t esp;
    uint64_t eflags;
    uint64_t eip;
    uint64_t cs;
    uint64_t ds;
    uint64_t ss;
    uint64_t intno;
    uint64_t errno;
};

#elif defined(__x86_64__) || defined(__amd64__) || defined(__amd64) || defined(__x86_64)

struct user_regs_struct {
    uint64_t rax;
    uint64_t rbx;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t rbp;
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t rip;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t cs;
    uint64_t ds;
    uint64_t ss;
    uint64_t int_no;
    uint64_t err_code;
};

#else
#error "You must add your architecture to user.h"
#endif

#endif