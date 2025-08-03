/**
 * @file libpolyhedron/wchar/wcsrchr.c
 * @brief wcsrchr
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

wchar_t *wcsrchr(const wchar_t *ws, wchar_t wc) {
    wchar_t *r = NULL;
    wchar_t *p = (wchar_t*)ws;
    while (*p) {
        if (*p == wc) r = p;
        p++;
    }

    return r;
}