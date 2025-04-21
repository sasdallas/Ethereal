/**
 * @file libpolyhedron/arch/x86_64/math/pow.c
 * @brief pow function
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <math.h>

double pow(double x, double y) {
    // WARNING WARNING WARNING WARNING
    // This is ass - kernel needs FPU support
    // WARNING WARNING WARNING WARNING

    long long pow = 1;
    for (int i = 0; i < y; i++) {
        pow *= x;
    }

    return pow;
}