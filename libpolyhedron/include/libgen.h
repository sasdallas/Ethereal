/**
 * @file libpolyhedron/include/libgen.h
 * @brief libgen
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

#ifndef _LIBGEN_H
#define _LIBGEN_H

/**** INCLUDES ****/
#include <stdint.h>

/**** FUNCTIONS ****/

char *dirname(char *path);
char *basename(char *path);

#endif

_End_C_Header;