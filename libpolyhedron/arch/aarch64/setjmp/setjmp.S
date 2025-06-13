/**
 * @file libpolyhedron/arch/aarch64/setjmp/setjmp.S
 * @brief setjmp and longjmp
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

.global setjmp
setjmp:
    // Store the stack in the frame
    mov x2, sp
    str x2, [x0], #8

    // Now we need to store the other preserved registers
    stp x19, x20, [x0], #16
    stp x21, x22, [x0], #16
    stp x23, x24, [x0], #16
    stp x25, x26, [x0], #16
    stp x27, x28, [x0], #16
    stp x29, x30, [x0], #16  // LR and FP

    mov x0, 0
    ret

.global longjmp
longjmp:
    // Grab the stack
    ldr x2, [x0], #8

    // Restore the other registers
    ldp x19, x20, [x0], #16
    ldp x21, x22, [x0], #16
    ldp x23, x24, [x0], #16
    ldp x25, x26, [x0], #16
    ldp x27, x28, [x0], #16
    ldp x29, x30, [x0], #16  // LR and FP

    mov sp, x2
    mov x0, 1
    ret
