/**
 * @file libpolyhedron/string/strcoll.c
 * @brief strcoll
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <string.h>

int strcoll(const char *s1, const char *s2) {
    // TODO: Handle LC_COLLATE?
    return strcmp(s1, s2);
}