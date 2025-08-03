/**
 * @file libpolyhedron/wchar/wcscpy.c
 * @brief wcscpy
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

wchar_t *wcscpy(wchar_t *ws1, const wchar_t *ws2) {
    while (*ws2) *ws1++ = *ws2++;
    *ws1 = L'\0';
    return ws1;
}

wchar_t *wcsncpy(wchar_t *ws1, const wchar_t *ws2, size_t n) {
    size_t rem = n;

    while (rem && *ws2) {
        *ws1++ = *ws2++;
        rem--;
    }

    while (rem) *ws1++ = 0;
    
    return &ws1[n-rem];
}