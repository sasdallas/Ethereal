/**
 * @file hexahedron/include/kernel/arch/i386/registers.h
 * @brief i386 registers
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_ARCH_I386_REGISTERS_H
#define KERNEL_ARCH_I386_REGISTERS_H

/**** INCLUDES ****/
#include <stdint.h>


/**** TYPES ****/

// Descriptor (e.g. gdtr, idtr)
typedef struct _descriptor {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) descriptor_t;

// Registers (Used by interrupts & exceptions)
// Some of these are pushed by the CPU
typedef struct _registers {
    uint16_t ds;                                        // NOTE: This is restored to aall segment registers (DS/ES/FS/GS) - bad?
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;    // Pushed by pusha

    // Pushed by the wrapper
    uint32_t int_no;
    uint32_t err_code;

    // Pushed by the CPU
    uint32_t eip, cs, eflags, useresp, ss;
} __attribute__((packed)) registers_t;

// Extended registers. Interrupts/exceptions will push this.
typedef struct _extended_registers {
    uint32_t cr0;
    uint32_t cr2;
    uint32_t cr3;
    uint32_t cr4;
    descriptor_t gdtr;
    descriptor_t idtr;
} __attribute__((packed)) extended_registers_t;


#endif