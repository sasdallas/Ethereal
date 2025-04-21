/**
 * @file libpolyhedron/include/sys/ioctl.h
 * @brief ioctl
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

_Begin_C_Header;

#ifndef _SYS_IOCTL_H
#define _SYS_IOCTL_H

/**** INCLUDES ****/
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>

/**** FUNCTIONS ****/

int ioctl(int fd, unsigned long request, ...);

#endif

_End_C_Header;