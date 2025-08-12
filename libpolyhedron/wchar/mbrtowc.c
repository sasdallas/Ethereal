/**
 * @file libpolyhedron/wchar/mbrtowc.c
 * @brief mbrtowc
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <stddef.h>
#include <stdlib.h>
#include <wchar.h>
#include <errno.h>

size_t mbrtowc(wchar_t *pwc, const char *s, size_t n, mbstate_t *ps) {
    if (!s) return 0;

    // TODO: polyhedron would benefit from a UTF-8 utility function
    size_t byte_length = 0;

    if ((s[0] & 0x80) == 0x00) {
        byte_length = 1;
    } else if ((s[0] & 0xe0) == 0xc0) {
        byte_length = 2;
    } else if ((s[0] & 0xf0) == 0xe0) {
        byte_length = 3;
    } else if ((s[0] & 0xf8) == 0xf0) {
        byte_length = 4;
    }

    if (!byte_length || n < byte_length) {
        errno = EILSEQ;
        return -1;
    }
    
    wchar_t codepoint = 0;

    switch (byte_length) {
        case 1: codepoint = (uint8_t)s[0]; break;
        case 2: codepoint = (((uint8_t)s[0] & 0x1F) << 6) | ((uint8_t)s[1] & 0x3F); break;
        case 3: codepoint = (((uint8_t)s[0] & 0x0F) << 12) | (((uint8_t)s[1] & 0x3F) << 6) | ((uint8_t)s[2] & 0x3F); break;
        case 4: codepoint = (((uint8_t)s[0] & 0x07) << 18) | (((uint8_t)s[1] & 0x3F) << 12) | (((uint8_t)s[2] & 0x3F) << 6) | (((uint8_t)s[3] & 0x3F)); break;
    }

    if (pwc) *pwc = codepoint;
    if (codepoint == 0) return 0;

    return byte_length;
}