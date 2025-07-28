/**
 * @file libpolyhedron/arch/x86_64/string/memset.S
 * @brief memset
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

.global memset
memset:
    movq %rdi, %rax     // save RDI in return register
    movb %sil, %al      // sil = lower bits of RSI
    movq %rdx, %rcx     // rdx = size

    rep stosb
    ret
