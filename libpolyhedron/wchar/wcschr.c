/**
 * @file libpolyhedron/wchar/wcschr.c
 * @brief wcschr
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

wchar_t *wcschr(const wchar_t *ws, wchar_t wc) {
    wchar_t *p = (wchar_t*)ws;
    if (wc == 0) return (p + wcslen(ws));
    
    while (*p) {
        if (*p == wc) return p;
        p++;
    }

    return NULL;
}

wchar_t *wmemchr(const wchar_t *ws, wchar_t wc, size_t n) {
    wchar_t *p = (wchar_t*)ws;
    while (n) {
        if (*p == wc) return p;
        n--;
    }

    return NULL;
}