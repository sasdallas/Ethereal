/**
 * @file libpolyhedron/locale/setlocale.c
 * @brief setlocale
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <sys/libc_debug.h>
#include <locale.h>
#include <stdio.h>

char *setlocale(int category, const char *locale) {
    dprintf("setlocale (stub): %d %s\n", category, locale);
    return "en_US"; // Default
}