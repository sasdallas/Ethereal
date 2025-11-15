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

struct thread;

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
 * 
 * Equivalent to the C function for longjmp
 */
__attribute__((noreturn)) void arch_load_context(struct arch_context *context);

/**
 * @brief Yield
 * 
 * Thank you to @hyenasky for the idea of BOTh unlocks
 * When you want to yield, leave the current CPU's queue locked after adding in the previous thread.
 * After getting off of the previous stack in use, this will unlock the scheduler's queue.
 * It will also unlock the prev sleep queue
 */
__attribute__((noreturn)) void arch_yield(struct thread *prev, struct thread *next);

/**
 * @brief Enter kernel thread
 * 
 * Pop these from the stack in this order:
 * 1. kthread pointer
 * 2. data value
 */
extern void arch_enter_kthread();

/**
 * @brief Restore context from @c registers_t structure
 * 
 * The registers at the time of system call are pushed onto the stack. Pop them in your usual order
 */
extern void arch_restore_context();

/**** MACROS ****/

#define IP(context) (context.rip)
#define SP(context) (context.rsp)
#define BP(context) (context.rbp)
#define TLSBASE(context) (context.fsbase)

#define REGS_IP(regs) (regs->rip)
#define REGS_SP(regs) (regs->rsp)
#define REGS_BP(regs) (regs->rbp)

#define ARCH_SYSCALL_REGNUM(regs) (regs->rax)
#define ARCH_SYSCALL_REGRET(regs) (regs->rax)
#define ARCH_SYSCALL_REG0(regs) (regs->rdi)
#define ARCH_SYSCALL_REG1(regs) (regs->rsi)
#define ARCH_SYSCALL_REG2(regs) (regs->rdx)
#define ARCH_SYSCALL_REG3(regs) (regs->r10)
#define ARCH_SYSCALL_REG4(regs) (regs->r8)
#define ARCH_SYSCALL_REG5(regs) (regs->r9)

#endif