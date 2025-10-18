/**
 * @file hexahedron/include/kernel/arch/x86_64/context.h
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

#ifndef KERNEL_ARCH_X86_64_CONTEXT_H
#define KERNEL_ARCH_X86_64_CONTEXT_H

/**** INCLUDES ****/
#include <stdint.h>
#include <kernel/arch/arch.h>

/**** TYPES ****/
typedef struct arch_context {
    uintptr_t rsp;      // Stack pointer
    uintptr_t rbp;      // Base pointer (TODO: We probably shouldn't be reloading this)
    
    uintptr_t rbx;      // RBX
    uintptr_t r12;      // R12
    uintptr_t r13;      // R13
    uintptr_t r14;      // R14
    uintptr_t r15;      // R15

    uintptr_t fsbase;   // FSBASE (TLS)

    uintptr_t rip;      // Instruction pointer
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
 * @param context The context to switch to
 * @param unlock_queue Unlock the CPU's thread queue.
 * 
 * Equivalent to the C function for longjmp
 * 
 * Thank you to @hyenasky for the idea of @c unlock_queue
 * When you want to yield, leave the current CPU's queue locked after adding in the previous thread.
 * After getting off of the previous stack in use, this will unlock the queue if @c unlock_queue is set.
 */
__attribute__((noreturn)) void arch_load_context(struct arch_context *context, int unlock_queue);

/**** MACROS ****/

#define IP(context) (context.rip)
#define SP(context) (context.rsp)
#define BP(context) (context.rbp)
#define TLSBASE(context) (context.fsbase)

#define REGS_IP(regs) (regs->rip)
#define REGS_SP(regs) (regs->rsp)
#define REGS_BP(regs) (regs->rbp)

#endif