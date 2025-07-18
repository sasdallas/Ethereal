/**
 * @file libpolyhedron/wchar/wcstombs.c
 * @brief wcstombs
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <stdlib.h>
#include <string.h>

size_t wcstombs (char* dest, const wchar_t* src, size_t max) {
    size_t written = 0;
    wchar_t *p = (wchar_t*)src;

    while (*p && written < max) {
        unsigned int ch = (unsigned int)*p;

        // Setup the codepoint
        if (ch <= 0x7F) {
            // 1-byte UTF-8
            if (written + 1 > max) return strlen(dest);
            
            if (dest) {
                dest[written] = (char)ch;
            }
            
            written += 1;
        } else if (ch <= 0x7FF) {
            // 2-byte UTF-8
            if (written + 2 > max) return strlen(dest);

            if (dest) {
                dest[written] = (char)(0xC0 | ((ch >> 6) & 0x1F));
                dest[written+1] = (char)(0x80 | (ch & 0x3F));
            }

            written += 2;
        } else if (ch <= 0xFFFF) {
            // 3-byte UTF-8
            if (written + 3 > max) return strlen(dest);

            if (dest) {
                dest[written] = (char)(0xE0 | ((ch >> 12) & 0x0F));
                dest[written+1] = (char)(0x80 | ((ch >> 6) & 0x3F));
                dest[written+2] = (char)(0x80 | (ch & 0x3F));
            }
             
            written += 3;
        } else if (ch <= 0x10FFFF) {
            // 4-byte UTF-8
            if (written + 4 > max) return strlen(dest);

            if (dest) {
                dest[written] = (char)(0xF0 | ((ch >> 18) & 0x07));
                dest[written+1] = (char)(0x80 | ((ch >> 12) & 0x3F));
                dest[written+2] = (char)(0x80 | ((ch >> 6) & 0x3F));
                dest[written+3] = (char)(0x80 | (ch & 0x3F));
            }

            written += 4;
        } else {
            return -1;
        }

        p++;
    }

    if (dest && written < max) dest[written] = 0;
    return written;
}