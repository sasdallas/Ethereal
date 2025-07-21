/**
 * @file libpolyhedron/include/sys/time.h
 * @brief 
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

#ifndef _SYS_TIME_H
#define _SYS_TIME_H


/**** INCLUDES ****/
#include <sys/types.h>
#include <sys/select.h>

/**** DEFINITIONS ****/

#define ITIMER_REAL         0
#define ITIMER_VIRTUAL      1
#define ITIMER_PROF         2

/**** TYPES ****/

// Seconds/microseconds timeval
struct timeval {
    time_t tv_sec;
    suseconds_t tv_usec;
};

// Timezone
struct timezone {
    int tz_minuteswest; // Minutes west of Greenwich
    int tz_dsttime;     // Type of Daylight Savings Time correction
};

struct itimerval {
    struct timeval it_interval;     // Timer interval
    struct timeval it_value;        // Current value
};

/**** MACROS ****/

#define timeradd(a, b, c) ({ \
                                (c)->tv_sec = (a)->tv_sec + (b)->tv_sec; \
                                (c)->tv_usec = (a)->tv_usec + (b)->tv_usec; \
                                (c)->tv_sec += (c)->tv_usec / 1000000; \
                                (c)->tv_usec %= 1000000; \
                            })

#define timersub(a, b, c) ({ \
                                (c)->tv_sec = (a)->tv_sec - (b)->tv_sec; \
                                (c)->tv_usec = (a)->tv_usec - (b)->tv_usec; \
                                (c)->tv_sec += (c)->tv_usec / 1000000; \
                                (c)->tv_usec %= 1000000; \
                            })

#define timerclear(a) ({ \
                            (a)->tv_sec = 0; \
                            (a)->tv_usec = 0; \
                        })

#define timerisset(a) ((a)->tv_sec || (a)->tv_usec)

#define timercmp(a, b, cmp) (((a)->tv_sec == (b)->tv_sec) ? ((a)->tv_usec cmp (b)->tv_usec) : ((a)->tv_sec cmp (b)->tv_sec)) 

/**** FUNCTIONS ****/
extern int gettimeofday(struct timeval *ptr, void *z);
extern int settimeofday(struct timeval *ptr, void *z);

int getitimer(int, struct itimerval *);
int setitimer(int, const struct itimerval *, struct itimerval*);
int utimes(const char *, const struct timeval[2]);

#ifdef __LIBK
extern unsigned long long now();
#endif

#endif

_End_C_Header