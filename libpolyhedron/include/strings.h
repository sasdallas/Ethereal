/**
 * @file libpolyhedron/include/strings.h
 * @brief strings
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

_Begin_C_Header

#ifndef STRINGS_H
#define STRINGS_H

/**** INCLUDES ****/
#include <stdint.h>
#include <stddef.h>

/**** FUNCTIONS ****/

int strcasecmp(const char *s1, const char *s2);
int strncasecmp(const char *s1, const char *s2, size_t n);

#endif

_End_C_Header