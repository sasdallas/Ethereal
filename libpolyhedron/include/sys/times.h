/**
 * @file libpolyhedron/include/sys/times.h
 * @brief Times header
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

#ifndef _SYS_TIMES_H
#define _SYS_TIMES_H

/**** INCLUDES ****/
#include <sys/times.h>


/**** TYPES ****/

struct tms {
    unsigned long tms_utime;            // CPU time spent executing instructions
    unsigned long tms_stime;            // CPU time spent executing inside the kernel
    unsigned long tms_cutime;           // Sum of tms_utime and tms_cutime for all waited-for terminated children
    unsigned long tms_cstime;           // Sum of tms_utime and tms_cutime for all waited-for terminated children
};

/**** FUNCTIONS ****/

clock_t times(struct tms *buf);

#endif

_End_C_Header