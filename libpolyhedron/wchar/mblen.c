/**
 * @file libpolyhedron/wchar/mblen.c
 * @brief mblen
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <wchar.h>

int mblen(const char *s, size_t n) {
    if (!s) return 0;
    if (!n) return -1;

    // TODO: Locales?

    if ((*s & 0x80) == 0x00) { if (n < 1) return -1; return 1; }
    if ((*s & 0xE0) == 0xC0) { if (n < 2) return -1; return 2; }
    if ((*s & 0xF0) == 0xE0) { if (n < 3) return -1; return 3; }
    if ((*s & 0xF8) == 0xF0) { if (n < 4) return -1; return 4; }

    return -1;
}