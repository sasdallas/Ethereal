/**
 * @file libpolyhedron/wchar/btowc.c
 * @brief btowc
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

wint_t btowc(int c) {
    if (c == 0 || c > 0x7F) return WEOF;
    return c;
}