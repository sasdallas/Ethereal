/**
 * @file libpolyhedron/arch/i386/crt/crt0.S
 * @brief crt0
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

.global _start

.extern __libc_main

_start:
    popq %rdi // argc
    popq %rsi // argv
    popq %rdx // envp

    mov $main, %rcx 
    callq __libc_main