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

typedef void *mcontext_t; // Does not hold anything

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