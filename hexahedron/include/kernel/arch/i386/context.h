/**
 * @file hexahedron/include/kernel/arch/i386/context.h
 * @brief Context header
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_ARCH_I386_CONTEXT_H
#define KERNEL_ARCH_I386_CONTEXT_H

/**** INCLUDES ****/
#include <stdint.h>

/**** TYPES ****/

// Context structure. This contains all registers that are preserved between calls to C functions
typedef struct arch_context {
    uintptr_t esp;  // Stack pointer
    uintptr_t ebp;  // Base pointer (TODO: We probably shouldn't be reloading this)
    uintptr_t ebx;  // EBX
    uintptr_t edi;  // EDI
    uintptr_t esi;  // ESI
    uintptr_t tls;  // TLS
    uintptr_t eip;  // Instruction pointer
} __attribute__((packed)) arch_context_t;

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

#define IP(context) (context.eip)
#define SP(context) (context.esp)
#define BP(context) (context.ebp)
#define TLSBASE(context) (context.tls)

#define REGS_IP(regs) (regs->eip)
#define REGS_SP(regs) (regs->esp)
#define REGS_BP(regs) (regs->ebp)

#endif