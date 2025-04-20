/**
 * @file libpolyhedron/include/sys/utsname.h
 * @brief utsname
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of reduceOS.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef _SYS_UTSNAME_H
#define _SYS_UTSNAME_H

/**** INCLUDES ****/
#include <stdint.h>

/**** DEFINITIONS ****/

/* POSIX doesnt do anything about this, sooo */
#define LIBPOLYHEDRON_UTSNAME_LENGTH    128

/**** TYPES ****/

struct utsname {
    char sysname[LIBPOLYHEDRON_UTSNAME_LENGTH];
    char nodename[LIBPOLYHEDRON_UTSNAME_LENGTH];
    char release[LIBPOLYHEDRON_UTSNAME_LENGTH];
    char version[LIBPOLYHEDRON_UTSNAME_LENGTH];
    char machine[LIBPOLYHEDRON_UTSNAME_LENGTH];
};

/**** FUNCTIONS ****/

int uname(struct utsname *buf);

#endif