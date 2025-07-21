/**
 * @file libpolyhedron/unistd/alarm.c
 * @brief alarm
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <unistd.h>
#include <sys/time.h>

unsigned alarm(unsigned seconds) {
    struct itimerval value, ovalue;

    value.it_value.tv_sec = seconds;
    value.it_value.tv_usec = 0;

    value.it_interval.tv_sec = 0;
    value.it_interval.tv_usec = 0;

    ovalue.it_value.tv_sec = 0;

    setitimer(ITIMER_REAL, (const struct itimerval*)&value, &ovalue);
    return ovalue.it_value.tv_sec;
}