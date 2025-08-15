/**
 * @file hexahedron/klib/time/time.c
 * @brief time
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
#include <kernel/drivers/clock.h>

time_t time(time_t *tloc) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    if (tloc) *tloc = tv.tv_sec;
    return tv.tv_sec; 
}

unsigned long long now() { return (uint64_t)time(NULL); }
int gettimeofday(struct timeval *tp, void *tzp) { return clock_gettimeofday(tp, tzp); }
int settimeofday(struct timeval *tp, void *tzp) { return clock_settimeofday(tp, tzp); }