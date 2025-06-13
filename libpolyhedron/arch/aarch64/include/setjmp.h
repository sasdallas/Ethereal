/**
 * @file libpolyhedron/arch/aarch64/include/setjmp.h
 * @brief setjmp
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

#ifndef _SETJMP_H
#define _SETJMP_H

/**** INCLUDES ****/
#include <stdint.h>

/**** DEFINITIONS ****/
#define _JBLEN              32

/**** TYPES ****/

typedef long long jmp_buf[_JBLEN];

/**** FUNCTIONS ****/

void   longjmp(jmp_buf j, int r);
int    setjmp(jmp_buf j);

#endif

_End_C_Header;