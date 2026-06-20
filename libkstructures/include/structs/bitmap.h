/**
 * @file libkstructures/include/structs/bitmap.h
 * @brief Header-only
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _STRUCTS_BITMAP_H
#define _STRUCTS_BITMAP_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// This follows something similar to Linux's bitmap implementation
#define BITMAP_DEFINE(name, nbits) unsigned long name[(nbits + sizeof(long) - 1) / sizeof(long)];
#define BITMAP_BITS (sizeof(unsigned long) * 8)
#define BITMAP_TO_SIZE(nbits) ((nbits + sizeof(unsigned char) - 1) / sizeof(unsigned char))

bool bitmap_test(unsigned long *b, size_t i);
void bitmap_set(unsigned long *b, size_t i);
void bitmap_clear(unsigned long *b, size_t i);
void bitmap_clear_range(unsigned long *b, size_t start, size_t end);
void bitmap_fill(unsigned long *b, unsigned char f, size_t n);
int bitmap_find_first(const unsigned long *b, size_t n);

#endif
