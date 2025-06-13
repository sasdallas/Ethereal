/**
 * @file hexahedron/include/kernel/arch/aarch64/context.h
 * @brief Context header
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef KERNEL_ARCH_AARCH64_CONTEXT_H
#define KERNEL_ARCH_AARCH64_CONTEXT_H

/**** INCLUDES ****/
#include <stdint.h>
#include <kernel/arch/arch.h>


/**** TYPES ****/

typedef struct arch_context {
    uintptr_t sp;                       // Stack pointer

    uintptr_t r19;
    uintptr_t r20;
    uintptr_t r21;
    uintptr_t r22;
    uintptr_t r23;
    uintptr_t r24;
    uintptr_t r25;
    uintptr_t r26;
    uintptr_t r27;
    uintptr_t r28;

    uintptr_t tpidr;                    // Thread ID
    uintptr_t elr;                      // Exception link register
    uintptr_t spsr;                     // Saved program status register
    uintptr_t lr;                       // Link register (IP)
} arch_context_t;

/**** FUNCTIONS ****/

/**
 * @brief Jump to usermode and execute at an entrypoint
 * @param entrypoint The entrypoint
 * @param stack The stack to use
 */
__attribute__((noreturn)) void arch_start_execution(uintptr_t entrypoint, uintptr_t stack);

/**
 * @brief Save the current thread context
 * 
 * Equivalent to the C function for setjmp
 */
__attribute__((returns_twice)) int arch_save_context(struct arch_context *context);

/**
 * @brief Load the current thread context
 * 
 * Equivalent to the C function for longjmp
 */
__attribute__((noreturn)) void arch_load_context(struct arch_context *context);

/**** MACROS ****/

// !!!: To be fixed
#define IP(context) (context.lr)
#define SP(context) (context.sp)
#define BP(context) (context.lr)

#define REGS_IP(regs) (regs->lr)
#define REGS_SP(regs) (regs->sp)
#define REGS_BP(regs) (regs->fp)

#endif