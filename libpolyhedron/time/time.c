/**
 * @file libpolyhedron/time/time.c
 * @brief Time functions. Provides time() and difftime()
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <time.h>
#include <sys/time.h>

time_t time(time_t *out) {
    struct timeval ptr;
    gettimeofday(&ptr, NULL);

    if (out) *out = ptr.tv_sec;

    return ptr.tv_sec;
}

double difftime(time_t a, time_t b) {
    return (double)(a - b);
}

