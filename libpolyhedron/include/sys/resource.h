/**
 * @file libpolyhedron/include/sys/resource.h
 * @brief sys/resource.h
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <sys/cheader.h>

_Begin_C_Header

#ifndef _SYS_RESOURCE_H
#define _SYS_RESOURCE_H

/**** INCLUDES ****/
#include <sys/types.h>
#include <sys/time.h>

/**** TYPES ****/

typedef unsigned int rlim_t;


struct rlimit {
    rlim_t rlim_cur;            // The current (soft) limit
    rlim_t rlim_max;            // The hard limit.
};

struct rusage {
    struct timeval ru_utime;    // User time used
    struct timeval ru_stime;    // System time used
};

/**** DEFINITIONS ****/

#define PRIO_PROCESS    0
#define PRIO_PGRP       1
#define PRIO_USER       2

#define RLIMIT_CORE     0
#define RLIMIT_CPU      1
#define RLIMIT_DATA     2
#define RLIMIT_FSIZE    3
#define RLIMIT_NOFILE   4
#define RLIMIT_STACK    5
#define RLIMIT_AS       6

#define RLIM_INFINITY   ((rlim_t))-1;
#define RLIM_SAVED_MAX  RLIM_INFINITY
#define RLIM_SAVED_CUR  RLIM_INFINITY

#define RUSAGE_SELF     0
#define RUSAGE_CHILDREN 1

/**** FUNCTIONS ****/


int  getpriority(int, id_t);
int  getrlimit(int, struct rlimit *);
int  getrusage(int, struct rusage *);
int  setpriority(int, id_t, int);
int  setrlimit(int, const struct rlimit *);

#endif

_End_C_Header