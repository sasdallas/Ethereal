/**
 * @file libpolyhedron/stdio/perror.c
 * @brief perror
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <stdio.h>
#include <errno.h>

void perror(const char *s) {
#ifndef __LIBK
    if (s && *s) {
        printf("%s: %s\n", s, strerror(errno));
    }
#endif
}