/**
 * @file hexahedron/arch/i386/signal.S
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
    pushl %eax
    pushl %ebx
    pushl %ecx
    pushl %edx
    pushl %edi
    pushl %esi
    pushl %ebp

    // On the stack we have:
    // 1. The signal handler
    // 2. The signal number
    // 3. The userspace return address
    movl 64(%esp), %rdi // Signal handler
    movl 72(%esp), %rax // Signal number

    // Align the stack
    movl %rsp, %rbp
    and $0xFFFFFFFFFFFFFFF0, %rsp

    // Leap of faith!
    call *%rax

    // We're back, restore.
    movl %rbp, %rsp
    popl %ebp
    popl %esi
    popl %edi
    popl %edx
    popl %ecx
    popl %ebx
    popl %eax

    // Update ESP
    addl $16, %esp

    ret $128