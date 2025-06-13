/**
 * @file hexahedron/arch/aarch64/bootstubs/qemu_virt/boot.S
 * @brief QEMU bootstub
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

.global _start

_start:
    ldr x30, =__stack_top
    mov sp, x30
    bl bootstub_main
    b .    