/**
 * @file hexahedron/include/kernel/arch/aarch64/interrupt.h
 * @brief ARM interrupts
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef KERNEL_ARCH_AARCH64_INTERRUPT_H
#define KERNEL_ARCH_AARCH64_INTERRUPT_H

/**** INCLUDES ****/
#include <stdint.h>
#include <kernel/arch/aarch64/registers.h>

/**** TYPES ****/

// Interrupt/exception handlers

/**
 * @brief Interrupt handler that accepts registers
 * @param exception_index The index of the exception
 * @param interrupt_no IRQ number
 * @param regs Registers
 * @param extended Extended registers (for debugging, will not be restored)
 * @return 0 on success, anything else is failure
 */
typedef int (*interrupt_handler_t)(uintptr_t exception_index, uintptr_t interrupt_no, registers_t* regs, extended_registers_t* extended);

/**
 * @brief Exception handler
 * @param exception_index The index of the exception
 * @param regs Registers
 * @param extended Extended registers (for debugging, will not be restored)
 * @return 0 on success, anything else is failure
 */
typedef int (*exception_handler_t)(uintptr_t exception_index, registers_t* regs, extended_registers_t* extended);

/**
 * @brief Interrupt handler with context
 * @param context The context given to @c hal_registerInterruptContext
 * @returns 0 on success
 */
typedef int (*interrupt_handler_context_t)(void *context);

#endif