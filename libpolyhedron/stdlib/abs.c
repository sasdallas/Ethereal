/**
 * @file libpolyhedron/stdlib/abs.c
 * @brief abs
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

int abs(int x) {
    return (x < 0) ? (x * -1) : x;
}