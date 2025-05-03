/**
 * @file libpolyhedron/stdio/rewind.c
 * @brief rewind
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

void rewind(FILE *stream) {
    fseek(stream, 0, SEEK_SET);
}