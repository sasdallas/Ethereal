/**
 * @file hexahedron/arch/x86_64/signal.S
 * @brief Signal trampoline
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

/* This code will be copied by the process launcher into the VAS */
.section .userspace

.global arch_signal_trampoline
arch_signal_trampoline:
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rdi
    pushq %rsi
    pushq %rbp
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    // On the stack we have:
    // 1. The userspace return address
    // 2. The signal number
    // 3. The signal handler
    movq 136(%rsp), %rax // Signal handler
    movq 128(%rsp), %rdi // Signal number

    // Align the stack
    movq %rsp, %rbp
    and $0xFFFFFFFFFFFFFFF0, %rsp

    // Leap of faith!
    call *%rax

    // We're back, restore.
    movq %rbp, %rsp
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rbp
    popq %rsi
    popq %rdi
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax

    ret $128