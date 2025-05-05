/**
 * @file libpolyhedron/include/sys/select.h
 * @brief select.h
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <sys/cheader.h>

_Begin_C_Header;

#ifndef _SYS_SELECT_H
#define _SYS_SELECT_H

/**** INCLUDES ****/
#include <stdint.h>
#include <sys/time.h>
#include <signal.h>
#include <time.h>

/**** DEFINITIONS ****/
#define FD_SETSIZE 64

/**** TYPES ****/
typedef unsigned int fd_mask;
typedef struct _fd_set {
    fd_mask fds_bits[1];
} fd_set;

#endif

_End_C_Header;