/**
 * @file hexahedron/include/kernel/arch/x86_64/registers.h
 * @brief Registers structure for x86_64
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_ARCH_X86_64_REGISTERS_H
#define KERNEL_ARCH_X86_64_REGISTERS_H

/**** INCLUDES ****/
#include <stdint.h>

/***** TYPES ****/

// Descriptor (e.g. gdtr, idtr)
typedef struct _descriptor {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) descriptor_t;

// Registers structure 
typedef struct _registers {
    uint16_t ds;

    uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;

    // Pushed by the wrapper
    uintptr_t int_no;
    uintptr_t err_code;
    
    // Pushed by the CPU
    uintptr_t rip, cs, rflags, rsp, ss;
} __attribute__((packed)) registers_t;

// Extended registers
typedef struct _extended_registers {
    uint64_t cr0;
    uint64_t cr2;
    uint64_t cr3;
    uint64_t cr4;
    descriptor_t gdtr;
    descriptor_t idtr;
} __attribute__((packed)) extended_registers_t;

#endif