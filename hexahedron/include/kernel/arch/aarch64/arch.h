/**
 * @file hexahedron/include/kernel/arch/aarch64/arch.h
 * @brief aarch64 arch.h
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef KERNEL_ARCH_AARCH64_ARCH_H
#define KERNEL_ARCH_AARCH64_ARCH_H

/**** INCLUDES ****/
#include <stdint.h>
#include <kernel/generic_mboot.h>
#include <kernel/arch/arch.h>
#include <kernel/arch/aarch64/registers.h>
#include <kernel/arch/aarch64/smp.h>

/**** DEFINITIONS ****/

#define ARCH_SYSCALL_NUMBER     0

/**** FUNCTIONS ****/

/**
 * @brief Say hi! Prints the versioning message and ASCII art to NOHEADER dprintf
 * @param is_debug Print to regular printf or dprintf
 */
void arch_say_hello(int is_debug);

#endif