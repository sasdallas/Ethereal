/**
 * @file hexahedron/include/kernel/arch/arch.h
 * @brief Provides general architecture-specific definitions.
 * 
 * Certain architectures need to expose certain functions. All of that is contained in this file.
 * Each function should have three things:
 * - A clear description of what it takes
 * - A clear description of what it does
 * - A clear description of what it returns
 * 
 * @note Includes HAL definitions as well!
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_ARCH_H
#define KERNEL_ARCH_H

/**** INCLUDES ****/
#include <stdint.h>
#include <kernel/generic_mboot.h>

/**** TYPES ****/

#if defined(__ARCH_I386__)
#include <kernel/arch/i386/cpu.h>
#include <kernel/arch/i386/registers.h>
#include <kernel/arch/i386/context.h>
#include <kernel/arch/i386/registers.h>
#include <kernel/arch/i386/hal.h>
#elif defined(__ARCH_X86_64__)
#include <kernel/arch/x86_64/cpu.h>
#include <kernel/arch/x86_64/registers.h>
#include <kernel/arch/x86_64/context.h>
#include <kernel/arch/x86_64/registers.h>
#include <kernel/arch/x86_64/hal.h>
#else
#error "Please define your includes here"
#endif


/**** TYPES ****/

struct thread; // Prototype
struct _registers;
struct _extended_registers;

/**** FUNCTIONS ****/

/**
 * @brief Prepare the architecture to enter a fatal state. This means cleaning up registers,
 * moving things around, whatever you need to do
 */
extern void arch_panic_prepare();

/**
 * @brief Finish handling the panic, clean everything up and halt.
 * @note Does not return
 */
extern void arch_panic_finalize();

/**
 * @brief Get the generic parameters
 */
extern generic_parameters_t *arch_get_generic_parameters();

/**
 * @brief Returns the current CPU ID active in the system
 */
extern int arch_current_cpu();

/**
 * @brief Pause execution on the current CPU for one cycle
 */
void arch_pause();

/**
 * @brief Determine whether the interrupt fired came from usermode 
 * 
 * Useful to main timer logic to know when to switch tasks.
 */
int arch_from_usermode(struct _registers *registers, struct _extended_registers *extended);

/**
 * @brief Prepare to switch to a new thread
 * @param thread The thread to prepare to switch to
 */
void arch_prepare_switch(struct thread *thread);

/**
 * @brief Initialize the thread context
 * @param thread The thread to initialize the context for
 * @param entry The requested entrypoint for the thread
 * @param stack The stack to use for the thread
 */
void arch_initialize_context(struct thread *thread, uintptr_t entry, uintptr_t stack);

/**
 * @brief Jump to usermode and execute at an entrypoint
 * @param entrypoint The entrypoint
 * @param stack The stack to use
 */
extern __attribute__((noreturn))  void arch_start_execution(uintptr_t entrypoint, uintptr_t stack);

/**
 * @brief Save the current thread context
 * 
 * Equivalent to the C function for setjmp
 */
extern __attribute__((returns_twice)) int arch_save_context(struct arch_context *context);

/**
 * @brief Load the current thread context
 * 
 * Equivalent to the C function for longjmp
 */
extern __attribute__((noreturn)) void arch_load_context(struct arch_context *context);

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

/**
 * @brief Say hi!
 * @param is_debug Print to dprintf or printf
 */
void arch_say_hello(int is_debug);

#endif