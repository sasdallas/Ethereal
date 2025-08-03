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
#include <stddef.h>
#include <sys/types.h>
#include <sys/time.h>

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

    const char* _tm_zone_name;
    int _tm_zone_offset;
    long int tm_gmtoff;
};

struct timespec {
    time_t tv_sec;
    long tv_nsec;
};

struct itimerspec {
    struct timespec it_interval;
    struct timespec it_value;
};

/**** DEFINITIONS ****/

#define CLOCK_MONOTONIC             0
#define CLOCK_PROCESS_CPUTIME_ID    1
#define CLOCK_REALTIME              2
#define CLOCK_THREAD_CPUTIME_ID     3

#define TIMER_ABSTIME           0x1

#define CLOCKS_PER_SEC          ((clock_t)1000000)
#define USEC_PER_SEC            ((useconds_t)1000000)

/**** MACROS ****/

#define timersub(a, b, res) { \
                                (res)->tv_sec = (a)->tv_sec - (b)->tv_sec; \
                                (res)->tv_usec = (a)->tv_usec - (b)->tv_usec; \
                                (res)->tv_sec += (res)->tv_usec / USEC_PER_SEC; \
                                (res)->tv_usec %= USEC_PER_SEC; \
                            }

/**** VARIABLES ****/

extern int	daylight;
extern long	timezone;
extern char* tzname[];


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

void tzset();

int clock_gettime(clockid_t clockid, struct timespec *res);

clock_t clock();

#endif

_End_C_Header