/**
 * @file libpolyhedron/arch/x86_64/setjmp/setjmp.S
 * @brief setjmp and longjmp
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

.global setjmp
setjmp:
    // Get the stack frame and load it into RDX
    leaq 8(%rsp), %rdx

    // Start setting up jmp_buf
    movq %rdx, 0(%rdi)              // RSP
    movq %rbp, 8(%rdi)              // RBP
    movq (%rsp), %rdx               // Load return address into RAX
    movq %rdx, 16(%rdi)             // Return address
    movq %rbx, 24(%rdi)             // RBX
    movq %r12, 32(%rdi)             // R12
    movq %r13, 40(%rdi)             // R13
    movq %r14, 48(%rdi)             // R14
    movq %r15, 56(%rdi)             // R15
    xor %rax, %rax
    retq

.global longjmp
longjmp:
    // Restore
    movq 0(%rdi), %rsp              // RSP
    movq 8(%rdi), %rbp              // RBP
    movq 16(%rdi), %rdx             // Return address
    movq 24(%rdi), %rbx             // RBX
    movq 32(%rdi), %r12             // R12
    movq 40(%rdi), %r13             // R13
    movq 48(%rdi), %r14             // R14
    movq 56(%rdi), %r15             // R15

    // If value is 0, set it to 1
    testq %rsi, %rsi
    jnz .nonzero
    incq %rsi
.nonzero:
    movq %rax, %rsi
    jmpq *%rdx