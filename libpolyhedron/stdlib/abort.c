/**
 * @file libpolyhedron/stdlib/abort.c
 * @brief Abort function
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <stdlib.h>
#include <stdio.h>

__attribute__((__noreturn__)) void abort(void) {
#if defined(__LIBK)

#include <kernel/panic.h>
#include <kernel/debug.h>

    dprintf_module(ERR, "LIBPOLY", "abort() was called\n");
    kernel_panic(KERNEL_DEBUG_TRAP, "libpolyhedron");

#else
    printf("abort()\n");
#endif

    while (1);
    __builtin_unreachable();
} 