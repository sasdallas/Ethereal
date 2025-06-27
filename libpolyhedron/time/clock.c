/**
 * @file libpolyhedron/time/clock.c
 * @brief clock() function
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <time.h>
#include <stdio.h>

clock_t clock() {
    fprintf(stderr, "clock: stub\n");
    return (clock_t)0;
}