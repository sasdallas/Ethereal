/**
 * @file libpolyhedron/include/syslog.h
 * @brief syslog.h
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

#ifndef _SYSLOG_H
#define _SYSLOG_H

/**** DEFINITIONS ****/

#define LOG_PID             0x1
#define LOG_CONS            0x2
#define LOG_NDELAY          0x4
#define LOG_ODELAY          0x8
#define LOG_NOWAIT          0x10

#define LOG_KERN            0
#define LOG_USER            1
#define LOG_MAIL            2
#define LOG_NEWS            3
#define LOG_UUCP            4
#define LOG_DAEMON          5
#define LOG_AUTH            6
#define LOG_CRON            7
#define LOG_LPR             8
#define LOG_LOCAL0          9
#define LOG_LOCAL1          10
#define LOG_LOCAL2          11
#define LOG_LOCAL3          12
#define LOG_LOCAL4          13
#define LOG_LOCAL5          14
#define LOG_LOCAL6          15
#define LOG_LOCAL7          16

#define LOG_EMERG           0
#define LOG_ALERT           1
#define LOG_CRIT            2
#define LOG_ERR             3
#define LOG_WARNING         4
#define LOG_NOTICE          5
#define LOG_INFO            6
#define LOG_DEBUG           7

/**** FUNCTIONS ****/

void  closelog(void);
void  openlog(const char *, int, int);
int   setlogmask(int);
void  syslog(int, const char *, ...);

#endif

_End_C_Header