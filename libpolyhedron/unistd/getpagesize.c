/**
 * @file libpolyhedron/unistd/getpagesize.c
 * @brief getpagesize
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <unistd.h>
#include <limits.h>

#ifndef PAGE_SIZE
#define PAGE_SIZE       4096
#endif

int getpagesize() {
    return PAGE_SIZE;
}