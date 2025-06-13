/**
 * @file hexahedron/include/kernel/arch/aarch64/registers.h
 * @brief aarch64 registers
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef KERNEL_ARCH_AARCH64_REGISTERS_H
#define KERNEL_ARCH_AARCH64_REGISTERS_H

/**** INCLUDES ****/
#include <stdint.h>

/**** TYPES ****/

// Registers (Used by interrupts & exceptions)
typedef struct _registers {
    uint64_t x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10;
    uint64_t x11, x12, x13, x14, x15, x16, x17, x18, x19, x20;
    uint64_t x21, x22, x23, x24, x25, x26, x27, x28;

    uint64_t fp, lr, sp;
} __attribute__((packed)) registers_t;

// Extended registers
typedef struct _extended_registers {
    uint64_t ttbr1_el1, ttbr1_el0;
} __attribute__((packed)) extended_registers_t;

#endif