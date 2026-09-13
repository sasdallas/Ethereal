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
#include <stdbool.h>
#include <kernel/arch/arch.h>
#include <kernel/arch/x86_64/registers.h>
#include <sys/signal.h>

/**** DEFINITIONS ****/

#define MCONTEXT_REG_R8 0
#define MCONTEXT_REG_R9 1
#define MCONTEXT_REG_R10 2
#define MCONTEXT_REG_R11 3
#define MCONTEXT_REG_R12 4
#define MCONTEXT_REG_R13 5
#define MCONTEXT_REG_R14 6
#define MCONTEXT_REG_R15 7
#define MCONTEXT_REG_RDI 8
#define MCONTEXT_REG_RSI 9
#define MCONTEXT_REG_RBP 10
#define MCONTEXT_REG_RBX 11
#define MCONTEXT_REG_RDX 12
#define MCONTEXT_REG_RAX 13
#define MCONTEXT_REG_RCX 14
#define MCONTEXT_REG_RSP 15
#define MCONTEXT_REG_RIP 16
#define MCONTEXT_REG_EFL 17
#define MCONTEXT_REG_CSGSFS 18
#define MCONTEXT_REG_ERR 19
#define MCONTEXT_REG_TRAPNO 20
#define MCONTEXT_REG_OLDMASK 21
#define MCONTEXT_REG_CR2 22
#define MCONTEXT_NREG 23

/**** TYPES ****/

typedef struct arch_context {
    uintptr_t rsp;
    uintptr_t rbp;
    uintptr_t rbx;
    uintptr_t r12;
    uintptr_t r13;
    uintptr_t r14;
    uintptr_t r15;
    uintptr_t fsbase;
    uintptr_t gsbase;
    uintptr_t rip;
} arch_context_t;

struct thread;
struct signal_frame;

/**** MACROS ****/

#define IP(context) (context.rip)
#define SP(context) (context.rsp)
#define BP(context) (context.rbp)
#define TLSBASE(context) (context.fsbase)
#define GSBASE(context) (context.gsbase)

#define REGS_IP(regs) ((regs)->rip)
#define REGS_SP(regs) ((regs)->rsp)
#define REGS_BP(regs) ((regs)->rbp)
#define REGS_ARG0(regs) ((regs)->rdi)
#define REGS_ARG1(regs) ((regs)->rsi)
#define REGS_ARG2(regs) ((regs)->rdx)
#define REGS_RETVAL(regs) ((regs)->rax)

/**** FUNCTIONS ****/

/**
 * @brief Jump to usermode and execute at an entrypoint
 * @param entrypoint The entrypoint
 * @param stack The stack to use
 */
__attribute__((noreturn)) void arch_start_execution(uintptr_t entrypoint, uintptr_t stack);

/**
 * @brief Switch context to another thread
 * @param prev The previous thread being switched away from
 * @param new The new thread to switch to
 * @param requeue Whether to queue the old thread back in
 * @returns The thread to queue back in or NULL
 */
struct thread *arch_switch_context(struct thread *prev, struct thread *new, bool requeue);

/**
 * @brief Arch handle threadexit
 * 
 * This is a stupid hack for the process destruction system. It impersonates the idle thread
 * and then runs @c thread_safeExit on its stack to allow for the thread to be marked as dead.
 * 
 * There's a better method to do this, but I'm not in the mood (May 3 2026)
 */
__attribute__((noreturn)) void arch_handle_threadexit(struct thread *idle_thread, struct thread *this_thread);

/**
 * @brief Enter thread
 * 
 * Argument and function are on the stack
 */
extern void arch_thread_entry();

/**
 * @brief Restore context from @c registers_t structure
 * 
 * The registers at the time of system call are pushed onto the stack. Pop them in your usual order
 */
extern void arch_restore_context();

/**
 * @brief Prepare signal frame
 * @param thread The thread to prepare
 * @param signal Signal to setup
 * @param regs Registers to configure
 * @param handler Saved handler
 * @param restorer Saved restorer
 * @param flags sa_flags
 * @param blocked This is getting old
 */
int arch_prepare_signal_frame(struct thread *thread, int signal, registers_t *regs, uintptr_t handler, void *restorer, int flags, sigset_t blocked);

/**
 * @brief Restore a thread from a signal frame
 * @param thr The thread to restore
 * @param regs The register frame to restore into
 * @param uctx User context
 */
int arch_restore_signal_frame(struct thread *thr, struct _registers *regs, void *uctx);

#endif
