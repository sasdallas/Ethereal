/**
 * @file hexahedron/include/kernel/arch/i386/arch.h
 * @brief Provides basic architecture definitions (internal)
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_ARCH_I386_ARCH_H
#define KERNEL_ARCH_I386_ARCH_H

/**** INCLUDES ****/
#include <kernel/generic_mboot.h>
#include <kernel/multiboot.h>
#include <kernel/arch/i386/registers.h>
#include <kernel/arch/i386/context.h>

/**** DEFINITIONS ****/

#define ARCH_SYSCALL_NUMBER     128

/**** TYPES ****/

typedef struct _stack_frame {
    struct _stack_frame *nextframe;
    uintptr_t ip;
} stack_frame_t;

/**** FUNCTIONS ****/

/**
 * @brief Say hi! Prints the versioning message and ASCII art to NOHEADER dprintf
 * @param is_debug Print to regular printf or dprintf
 */
void arch_say_hello(int is_debug);


/**
 * @brief Parse a Multiboot 1 header and packs into a @c generic_parameters structure
 * @param bootinfo The Multiboot information
 * @returns A generic parameters structure
 */
generic_parameters_t *arch_parse_multiboot1(multiboot_t *bootinfo);

/** 
 * @brief Parse a Multiboot 2 header and packs into a @c generic_parameters structure
 * @param bootinfo A pointer to the multiboot informatino
 * @returns A generic parameters structure
 */
generic_parameters_t *arch_parse_multiboot2(multiboot_t *bootinfo);

/**
 * @brief Mark/unmark valid spots in memory
 * @todo Work in tandem with mem.h to allow for a maximum amount of blocks to be used
 */
void arch_mark_memory(generic_parameters_t *parameters, uintptr_t highest_address, uintptr_t mem_size);

/**
 * @brief Perform a stack trace using ksym
 * @param depth How far back to stacktrace
 * @param registers Optional registers
 */
void arch_panic_traceback(int depth, registers_t *regs);

#endif