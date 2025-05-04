/**
 * @file libpolyhedron/arch/i386/crt/crt0.S
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
    popq %rsi // argc
    popq %rdx // argv
    popq %rcx // envp

    // The kernel is stupid and doesn't usually align our stack
    // So align the stack to 16-byte boundary (SSE instructions and whatnot need this)
    and $0xFFFFFFFFFFFFFFF0, %rsp

    // Get main
    mov $main, %rdi 

    // Clear RBP
    xor %rbp, %rbp
    callq __libc_main