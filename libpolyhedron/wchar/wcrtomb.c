/**
 * @file libpolyhedron/wchar/wcrtomb.c
 * @brief wcrtomb
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
#include <errno.h>

size_t wcrtomb(char *s, wchar_t ws, mbstate_t *ps) {
    if (!s) return 1;

    unsigned int ch = (unsigned int)ws;

    // Determine the length
    if (ch <= 0x7F) {
        // 1-byte UTF-8
        s[0] = ch;
        return 1;
    } else if (ch <= 0x7FF) {
        // 2-byte UTF-8
        s[0] = (char)(0xC0 | ((ch >> 6) & 0x1F));
        s[1] = (char)(0x80 | (ch & 0x3F));
        return 2;
    } else if (ch <= 0xFFFF) {
        // 3-byte UTF-8
        s[0] = (char)(0xE0 | ((ch >> 12) & 0x0F));
        s[1] = (char)(0x80 | ((ch >> 6) & 0x3F));
        s[2] = (char)(0x80 | (ch & 0x3F));
        return 3;
    } else if (ch <= 0x10FFFF) {
        // 4-byte UTF-8
        s[0] = (char)(0xF0 | ((ch >> 18) & 0x07));
        s[1] = (char)(0x80 | ((ch >> 12) & 0x3F));
        s[2] = (char)(0x80 | ((ch >> 6) & 0x3F));
        s[3] = (char)(0x80 | (ch & 0x3F));
        return 4;
    }

    errno = EILSEQ;
    return -1;
}