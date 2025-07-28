/**
 * @file libpolyhedron/arch/x86_64/string/memcpy.S
 * @brief memcpy implementation
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

.global memcpy
memcpy:
    movq %rdi, %rax
    movq %rdx, %rcx
    movq %rdi, %rdx
    rep movsb 
    movq %rdx, %rax
    ret