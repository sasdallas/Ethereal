/**
 * @file hexahedron/arch/aarch64/signal.S
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
    /* stub */
    ret
