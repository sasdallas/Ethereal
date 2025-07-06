/**
 * @file libpolyhedron/include/sched.h
 * @brief sched
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

#ifndef _SCHED_H
#define _SCHED_H

/**** INCLUDES ****/
#include <time.h>

/**** DEFINITIONS ****/

#define SCHED_FIFO          0
#define SCHED_RR            1
#define SCHED_OTHER         2

/**** TYPES ****/

struct sched_param {
    int sched_priority;
};

/**** FUNCTIONS ****/

int    sched_get_priority_max(int);
int    sched_get_priority_min(int);
int    sched_getparam(pid_t, struct sched_param *);
int    sched_getscheduler(pid_t);
int    sched_rr_get_interval(pid_t, struct timespec *);
int    sched_setparam(pid_t, const struct sched_param *);
int    sched_setscheduler(pid_t, int, const struct sched_param *);
int    sched_yield(void);

#endif

_End_C_Header