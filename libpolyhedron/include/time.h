/**
 * @file libpolyhedron/include/time.h
 * @brief C timing
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <sys/cheader.h>

_Begin_C_Header

#ifndef _TIME_H
#define _TIME_H

/**** INCLUDES ****/
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/times.h>

/**** TYPES ****/

struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;

    // incompliant
    const char* _tm_zone_name;
    int _tm_zone_offset;
    long int tm_gmtoff;
};

/**** DEFINITIONS ****/

#define CLOCKS_PER_SEC 1000000

/**** FUNCTIONS ****/

extern struct tm *localtime(const time_t *ptr);
extern struct tm *localtime_r(const time_t *ptr, struct tm *buf);
extern struct tm *gmtime(const time_t *ptr);
extern struct tm *gmtime_r(const time_t *ptr, struct tm *buf);

extern size_t strftime(char *s, size_t max, const char *format, const struct tm *tm);
extern time_t mktime(struct tm *tm);
extern time_t time(time_t *out);
extern double difftime(time_t a, time_t b);
extern char *asctime(const struct tm *tm);
extern char *ctime(const time_t *ptr);

#endif

_End_C_Header