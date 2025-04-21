/**
 * @file libpolyhedron/string/strchr.c
 * @brief strchr/strrchr/strchrnul
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <string.h>

char *strchr(const char *str, int character) {
    char *ptr = (char*)str;

    while (*ptr) {
        if (*ptr == character) return ptr;
        ptr++;
    }

    return NULL;
}

char *strrchr(const char *str, int character) {
    char *occurence = NULL;

    char *ptr = (char*)str;
    while (*ptr) {
        if (*ptr == character) occurence = ptr;
        ptr++;
    }    

    return occurence;
}

char *strchrnul(const char *str, int character) {
    char *ptr = (char*)str;
    while (*ptr && *ptr != character) ptr++;
    return ptr;
}


