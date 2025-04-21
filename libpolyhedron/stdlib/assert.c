/**
 * @file libpolyhedron/stdlib/assert.c
 * @brief provides __assert_fail
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <assert.h>

void __assert_failed(char *file, int line, char *stmt) {
#if defined(__LIBK)
#include <kernel/panic.h>
   kernel_panic_extended(ASSERTION_FAILED, "libpoly", "*** Assertion (%s:%i) failed: %s\n", file, line, stmt);

#elif defined(__LIBC)
    printf("Assertion at %s:%i failed: %s\n", file, line, stmt);
    abort();
#endif

    while (1);
    __builtin_unreachable();
}