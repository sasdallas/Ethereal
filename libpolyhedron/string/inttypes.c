/**
 * @file libpolyhedron/string/strtoimax.c
 * @brief strtoimax/strtoumax
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <inttypes.h>
#include <stdlib.h>

intmax_t strtoimax(const char *nptr, char **endptr, int base) {
    return strtoll(nptr, endptr, base);
}

uintmax_t strtoumax(const char *nptr, char **endptr, int base) {
    return strtoull(nptr, endptr, base);
}