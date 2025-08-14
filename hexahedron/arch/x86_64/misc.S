/**
 * @file hexahedron/arch/x86_64/util.S
 * @brief Minor utility code
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

.set CR0_NE, 1 << 5
.set CR0_TS, 1 << 3
.set CR0_EM, 1 << 2
.set CR0_MP, 1 << 1
.set CR4_OSFXSR, 1 << 9
.set CR4_OSXMMEXCPT, 1 << 10

// Reference for SSE/FPU code: https://wiki.osdev.org/SSE and https://wiki.osdev.org/FPU
// x86_64 requires a minimum level of SSE support so we can ensure it

.global arch_enable_sse 
arch_enable_sse:
    // Despite the function name we will begin with FPU init
    mov %cr0, %rdx
    and $(~(CR0_TS | CR0_EM)), %rdx
    mov %rdx, %cr0
    fninit

    // Now we can do SSE initialization
    mov %cr0, %rax
    and $(~(CR0_EM)), %rax
    or $(CR0_MP), %rax
    mov %rax, %cr0
    mov %cr4, %rax
    or $(CR4_OSFXSR | CR4_OSXMMEXCPT), %rax
    mov %rax, %cr4

    // SSE enabled successfully
    ret