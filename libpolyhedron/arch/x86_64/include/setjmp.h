/**
 * @file libpolyhedron/arch/x86_64/include/setjmp.h
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
#define _JBLEN              9

/**** TYPES ****/

typedef long long jmp_buf[_JBLEN];
typedef long long sigjmp_buf[_JBLEN + 1 + (sizeof(long long) / sizeof(long))];

/**** FUNCTIONS ****/

void   longjmp(jmp_buf j, int r);
int    setjmp(jmp_buf j);

int     sigsetjmp(sigjmp_buf env, int savemask);
void    siglongjmp(sigjmp_buf env, int val);

#endif

_End_C_Header;