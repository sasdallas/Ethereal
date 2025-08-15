/**
 * @file hexahedron/klib/stdlib/assert.c
 * @brief assert
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <kernel/panic.h>

void __assert_failed(const char *file, int line, const char *stmt) {
    kernel_panic_extended(ASSERTION_FAILED, "klib", "*** Assertion (%s:%i) failed: %s\n", file, line, stmt);
    __builtin_unreachable();
}