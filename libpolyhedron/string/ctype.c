/**
 * @file libpolyhedron/string/ctype.c
 * @brief isprint, isalpha, isascii, etc.
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <ctype.h>

int isascii(int c) {
    return (c >= 0 && c <= 127);
}

int isalnum(int c) {
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

int isalpha(int c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

int iscntrl(int c) {
    return (c < ' '); // TODO: ?
}

int isdigit(int c) {
    return (c >= '0' && c <= '9');
}

int isgraph(int c) {
    return 0; // TODO: no
}

int islower(int c) {
    return (c >= 'a' && c <= 'z');
}

int isprint(int c) {
    return (c >= ' ' && c <= 0x7F); // 0x7F is [DEL]
}

int ispunct(int c) {
    // Puncutation can be layed out
    switch (c) {
        case '.':
        case '!':
        case '?':
        case ',':
            return 1;
        default:
            return 0;
    }
}

int isspace(int c) {
    return (c == ' ' || c == '\t' || c == '\v' || c == '\n' || c == '\r');
}

int isupper(int c) {
    return (c >= 'A' && c <= 'Z');
}

int isxdigit(int c) {
    // Hex goes from 0-F
    return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

int isblank(int c) {
    return (c == '\t' || c == ' ');
}


int toupper(int c) {
    if (islower(c)) return c - 0x20;
    return c;
}

int tolower(int c) {
    if (isupper(c)) return c + 0x20;
    return c;
}