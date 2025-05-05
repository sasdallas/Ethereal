/**
 * @file libpolyhedron/stdlib/rand.c
 * @brief Weak standard random number generator
 * 
 * Taken directly from the C standard document. Standard LCG.
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <stdlib.h>

#ifndef RAND_PROVIDED

static unsigned long __rand_next = 0;

void srand(unsigned int seed) {
    __rand_next = seed;
}

int rand() {
    // Assuming RAND_MAX is 32767
    __rand_next = __rand_next * 1103515245 + 12345;
    return (unsigned int)(__rand_next / 65536) % 32768;
}

#endif