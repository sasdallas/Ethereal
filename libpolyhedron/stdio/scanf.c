/**
 * @file libpolyhedron/stdio/scanf.c
 * @brief scanf
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <stdio.h>

int scanf(const char *format, ...) {
    printf("scanf: %s\n", format);
    return 0;
}

int fscanf(FILE *stream, const char *format, ...) {
    printf("fscanf %p %s\n", stream, format);
    return 0;
}

int sscanf(const char *str, const char *format, ...) {
    printf("sscanf \"%s\" \"%s\"", str, format);
    return 0;
}