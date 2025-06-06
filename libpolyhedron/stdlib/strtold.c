/**
 * @file libpolyhedron/stdlib/strtold.c
 * @brief strtold
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

long double strtold(const char *str, char **endptr) {
    return strtod(str, endptr);
}
