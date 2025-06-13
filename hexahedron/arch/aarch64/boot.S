/**
 * @file hexahedron/arch/aarch64/boot.S
 * @brief Bootstrap code
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

.section .bootstrap
.extern __stack_bottom:
.extern __stack_top:

.extern arch_main

.global _start
_start:
    ldr x30, =__stack_top
    mov sp, x30
    bl arch_main

.boot_hang:
    b .