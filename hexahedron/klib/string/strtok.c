/**
 * @file hexahedron/klib/string/strtok.c
 * @brief strtok
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

#define HAS_DELIM(ch) (!!memchr(delim, ch, delim_len))

char *strtok_r(char *str, const char *delim, char **saveptr) {
    if (!str) str = *saveptr;
    if (!(*str)) return NULL;
    size_t delim_len = strlen(delim);

    // Skip leading delimiters
    while (*str && HAS_DELIM(*str)) str++;
    if (!(*str)) return NULL;

    // Find the next delimiter
    char *start = str;
    while (*str && !HAS_DELIM(*str)) str++;

    if (*str) {
        *str = '\0'; // Null-terminate the token
        str++;
    }
    *saveptr = str;

    return start;
}

static char *__strtok_save = NULL;

char *strtok(char *str, const char *delim) {
    return strtok_r(str, delim, &__strtok_save);
}