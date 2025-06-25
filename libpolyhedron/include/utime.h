/**
 * @file libpolyhedron/include/utime.h
 * @brief utime.h
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

#ifndef _UTIME_H
#define _UTIME_H

/**** INCLUDES ****/
#include <sys/types.h>

/**** TYPES ****/

struct utimbuf {
    time_t actime;          // Access time
    time_t modtime;         // Modification time
};

/**** FUNCTIONS ****/

int utime(const char *, const struct utimbuf *);

#endif