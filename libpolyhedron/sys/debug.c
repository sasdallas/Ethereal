/**
 * @file libpolyhedron/sys/debug.c
 * @brief libc debug
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <sys/libc_debug.h>

/* Cache variable */
static int __libc_debug_enable = -1;

int __libc_debug_enabled() {
    if (__libc_debug_enable == -1) {
        char *v = getenv(LIBC_DEBUG_ENV);
        __libc_debug_enable = !!v; // TODO: Maybe check if it's 1
    }

    return __libc_debug_enable;
}
