/**
 * @file hexahedron/klib/string/ctype.c
 * @brief ctypes
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <ctype.h>

int tolower(int ch) { return (isupper(ch)) ? (ch + 0x20) : ch; }
int toupper(int ch) { return (islower(ch)) ? (ch - 0x20) : ch; }
int islower(int ch) { return ((ch >= 'a') && (ch <= 'z')); }
int isupper(int ch) { return ((ch >= 'A') && (ch <= 'Z')); }
int isdigit(int ch) { return ((ch >= '0') && (ch <= '9')); }
int isxdigit(int ch) { return isdigit(ch) || ((ch >= 'a') && (ch <= 'f')) || ((ch >= 'A') && (ch <= 'F')); }
int isprint(int ch) { return ((ch >= 0x20) && (ch <= 0x7F)); }
int isspace(int ch) { return (ch == ' ' || ch == '\f' || ch == '\n' || ch == '\r' || ch == '\t' || ch == '\v'); }