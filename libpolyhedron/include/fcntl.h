/**
 * @file libpolyhedron/include/fcntl.h
 * @brief File control options
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

#ifndef _FCNTL_H
#define _FCNTL_H

/**** INCLUDES ****/
#include <stdint.h>

/**** DEFINITIONS ****/

/* Open bitflags */
#define O_RDONLY        0x00000000
#define O_WRONLY        0x00000001
#define O_RDWR          0x00000002
#define O_CREAT         0x00000100
#define O_EXCL          0x00000200
#define O_NOCTTY        0x00000400
#define O_TRUNC         0x00001000
#define O_APPEND        0x00002000
#define O_NONBLOCK      0x00004000
#define O_DSYNC         0x00010000
#define O_DIRECT        0x00040000
#define O_LARGEFILE     0x00100000
#define O_DIRECTORY     0x00200000
#define O_NOFOLLOW      0x00400000
#define O_NOATIME       0x01000000
#define O_CLOEXEC       0x02000000
#define O_PATH          0x04000000

/* File bitflags are todo */

#endif