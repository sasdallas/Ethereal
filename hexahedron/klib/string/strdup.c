/**
 * @file hexahedron/klib/string/strdup.c
 * @brief strdup
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <string.h>
#include <stdlib.h>

char *strdup(const char *s) {
    size_t len = strlen(s);
    char *rep = malloc(len+1);
    memcpy(rep, s, len);
    rep[len] = 0;
    return rep;
}