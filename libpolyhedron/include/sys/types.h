/**
 * @file libpolyhedron/include/sys/types.h
 * @brief Types
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

#ifndef _SYS_TYPES_H
#define _SYS_TYPES_H

/**** INCLUDES ****/
#include <stdint.h>

/**** TYPES ****/

typedef int gid_t;
typedef int uid_t;
typedef int dev_t;
typedef int ino_t;
typedef int mode_t;
typedef int caddr_t;
typedef int pid_t;
typedef int id_t;

typedef long off_t;
typedef long time_t;
typedef long clock_t;

typedef unsigned long useconds_t;
typedef long suseconds_t;

typedef long ssize_t;

typedef unsigned short nlink_t;

typedef unsigned long blkcnt_t;
typedef unsigned long blksize_t;

#endif

_End_C_Header