/**
 * @file libpolyhedron/string/ffs.c
 * @brief ffs
 * 
 * Technically part of strings.h
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <strings.h>
#include <stddef.h>

int ffs(int i) {
    for (size_t j = 0; j < sizeof(int) * 8; j++) if (i & (1 << j)) return j+1;
    return 0;
}

int ffsl(long i) {
    for (size_t j = 0; j < sizeof(long) * 8; j++) if (i & (1 << j)) return j+1;
    return 0;
}

int ffsll(long long i) {
    for (size_t j = 0; j < sizeof(long long) * 8; j++) if (i & (1 << j)) return j+1;
    return 0;
}