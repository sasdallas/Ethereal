/**
 * @file libpolyhedron/stdlib/div.c
 * @brief div, ldiv, lldiv
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <stdlib.h>

div_t div(int x, int y) {
    div_t div = {
        .quot = x/y,
        .rem = x%y
    };
    return div;
}

ldiv_t ldiv(long x, long y) {
    ldiv_t ldiv = {
        .quot = x/y,
        .rem = x%y
    };
    return ldiv;
}

lldiv_t lldiv(long long x, long long y) {
    lldiv_t lldiv = {
        .quot = x/y,
        .rem = x%y
    };
    return lldiv;
}