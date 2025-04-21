/**
 * @file libpolyhedron/string/strtok.c
 * @brief strtok and strtok_r
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <string.h>

char *strtok_r(char *str, const char *seps, char **lasts) {
    // First check if we're using lasts instead of str
    if (!str) str = *lasts;
    if (!str) return NULL;

    // Now find the first occurence of seps with strspn
    str += strspn(str, (char*)seps);
    if (!(*str)) {
        // No occurence
        *lasts = str;
        return NULL;
    }

    // token now contains a pointer to the first occurence of seps
    char *token = str;

    str = strpbrk(token, (char*)seps);
    if (!str) {
        // Update lasts to point to the end of the string
        *lasts = (char*)((size_t)strchr(token, '\0'));
    } else {
        // There's still more occurences in the string, update.
        *str = '\0';
        *lasts = str + 1;
    }

    return token;
}

char *strtok(char *str, const char *seps) {
    // Forward call to strtok_r (restartable version)
    static char *lasts = NULL;
    return strtok_r(str, seps, &lasts);
}