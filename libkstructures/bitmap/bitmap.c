/**
 * @file libkstructures/bitmap/bitmap.c
 * @brief bitmap
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <structs/bitmap.h>
#include <string.h>

bool bitmap_test(unsigned long *b, size_t i) {
    return (b[i / BITMAP_BITS] & (1UL << (i % BITMAP_BITS))) != 0;
}
void bitmap_set(unsigned long *b, size_t i) {
    b[i / BITMAP_BITS] |= 1UL << (i % BITMAP_BITS);
}


void bitmap_clear_range(unsigned long *b, size_t start, size_t end) {
    // need to align start
    while ((start % BITMAP_BITS) != 0 && start < end) {
        bitmap_clear(b, start);
        start++;
    }

    if (start >= end) {
        return;
    }

    if (end - start > BITMAP_BITS) {
        memset(&b[start / BITMAP_BITS], 0, (end-start)/8);
        start += ((end-start)/8) * 8; // truncates the last bit off
    }

    // clean the last bit up
    while (start < end) {
        bitmap_clear(b, start);
        start++;
    }
}

void bitmap_clear(unsigned long *b, size_t i) {
    b[i / BITMAP_BITS] &= ~(1UL << (i % BITMAP_BITS));
}

void bitmap_fill(unsigned long *b, unsigned char f, size_t n) {
    size_t w = (n + BITMAP_BITS - 1) / BITMAP_BITS;
    memset(b, f, w * sizeof(unsigned long));
    if (n % BITMAP_BITS) b[w - 1] = (1UL << (n % BITMAP_BITS)) - 1;
}


int bitmap_find_first_from(const unsigned long *b, size_t start, size_t n) {
    if (start >= n) {
        return -1;
    }

    for (size_t i = start / BITMAP_BITS; i < (n + BITMAP_BITS - 1) / BITMAP_BITS; ++i) {
        unsigned long word = ~b[i];

        if (i == start / BITMAP_BITS) {
            word &= (~0UL << (start % BITMAP_BITS));
        }

        if (word) {
            size_t b = i * BITMAP_BITS + __builtin_ctzll(word);
            return (b < n) ? (int)b : -1;
        }
    }
    
    return -1;
}

int bitmap_find_first(const unsigned long *b, size_t n) {
    for (size_t i = 0, w = (n + BITMAP_BITS - 1) / BITMAP_BITS; i < w; ++i) {
        if (~b[i]) {
            return i * BITMAP_BITS + __builtin_ctzll(~b[i]);
        }
    }
    
    return -1;
}
