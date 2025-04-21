/**
 * @file libpolyhedron/include/sys/times.h
 * @brief Times header
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of Ethereal Operating System.
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
    unsigned long tms_utime;
    unsigned long tms_stime;
    unsigned long tms_cutime;
    unsigned long tms_cstime;
};

#endif

_End_C_Header