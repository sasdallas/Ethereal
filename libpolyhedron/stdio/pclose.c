/**
 * @file libpolyhedron/stdio/pclose.c
 * @brief pclose
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <stdio.h>
#include <stdlib.h>

int pclose(FILE *stream) {
    fprintf(stderr, "libc: pclose: stub\n");
    abort();
    return 0;;
}