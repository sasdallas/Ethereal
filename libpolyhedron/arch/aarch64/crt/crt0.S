/**
 * @file libpolyhedron/arch/aarch64/crt/crt0.S
 * @brief crt0
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

.global _start

.extern __libc_main

_start:
    ldr x1, [sp], #8
    ldr x2, [sp], #8
    ldr x3, [sp], #8
    ldr x4, [sp], #8

    // The kernel is stupid and doesn't usually align our stack
    // So align the stack to 16-byte boundary (SSE instructions and whatnot need this)
    mov x9, sp
    and x9, x9, #0xFFFFFFFFFFFFFFF0
    mov sp, x9

    // Get main
    ldr x0, =main
    bl __libc_main