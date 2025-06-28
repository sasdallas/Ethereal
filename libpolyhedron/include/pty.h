/**
 * @file libpolyhedron/include/pty.h
 * @brief pty
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

#ifndef _PTY_H
#define _PTY_H

/**** INCLUDES ****/
#include <termios.h>
#include <features.h>
#include <sys/ioctl.h>

/**** FUNCTIONS ****/

int openpty(int *amaster, int *aslave, char *name, const struct termios *termp, const struct winsize *winp);
pid_t forkpty(int *amaster, char *name, const struct termios *termp, const struct winsize *winp);

#endif

_End_C_Header