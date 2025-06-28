/**
 * @file libpolyhedron/include/sys/ucontext.h
 * @brief ucontext
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

#ifndef _SYS_UCONTEXT_H
#define _SYS_UCONTEXT_H

/**** INCLUDES ****/

#include <sys/signal.h>

/**** TYPES ****/

/* Registers */
/* For info on compiler target macros: https://sourceforge.net/p/predef/wiki/Architectures/ */
enum {
#ifdef __ARCH_I386__ || defined(__i686__) || defined(__i386__)
    REG_EAX,
    REG_EBX,
    REG_ECX,
    REG_EDX,
    REG_EDI,
    REG_ESI,
    REG_ESP,
    REG_EBP,
    REG_EIP
#elif defined(__ARCH_X86_64__) || defined(__x86_64__) || defined(__amd64__) || defined(__amd64) || defined(__x86_64)
    REG_RAX,
    REG_RBX,
    REG_RCX,
    REG_RDX,
    REG_RDI,
    REG_RSI,
    REG_RSP,
    REG_RBP,
    REG_RIP,
    REG_R8,
    REG_R9,
    REG_R10,
    REG_R11,
    REG_R12,
    REG_R13,
    REG_R14,
    REG_R15,
#elif defined(__ARCH_AARCH64__) || defined(__aarch64__)
    __TODO_LAZY,
#else
    #error "Please add architecture symbols to ucontext.h"
#endif

    __MCONTEXT_NUM_REGISTERS
};


typedef struct mcontext {
    long gregs[__MCONTEXT_NUM_REGISTERS];
} mcontext_t; 

typedef struct ucontext {
    struct ucontext *uc_link;       // Pointer to the context that will be resumed when this context returns
    sigset_t uc_sigmask;            // The set of signals that are blocked when this context is active
    stack_t uc_stack;               // The stack used by this context
    mcontext_t uc_mcontext;         // Machine context
} ucontext_t;

/**** FUNCTIONS ****/

int  getcontext(ucontext_t *uc);
int  setcontext(const ucontext_t *uc);
void makecontext(ucontext_t *uc, void (*func)(void), int argc, ...);
int  swapcontext(ucontext_t *uc, const ucontext_t *uvp);

#endif

_End_C_Header