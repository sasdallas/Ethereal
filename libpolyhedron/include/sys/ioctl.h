/**
 * @file libpolyhedron/include/sys/ioctl.h
 * @brief ioctl
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

_Begin_C_Header;

#ifndef _SYS_IOCTL_H
#define _SYS_IOCTL_H

/**** INCLUDES ****/
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>

/**** DEFINITIONS ****/

#define FIONBIO         0x8001      // Set to nonblocking mode

/* TTY ioctls */
#define IOCTLTTYIS          0x8002      // isatty
#define IOCTLTTYNAME        0x8003      // TTY name
#define IOCTLTTYLOGIN       0x8004      // Login to TTY

/**** FUNCTIONS ****/

int ioctl(int fd, unsigned long request, ...);

#endif

_End_C_Header;