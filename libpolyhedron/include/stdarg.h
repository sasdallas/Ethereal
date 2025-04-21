/**
 * @file libpolyhedron/include/stdarg.h
 * @brief Standard argument header
 * 
 * @warning This file is NOT FULLY compliant.
 * 			It is also freestanding, so not sure why we're doing this.
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

#ifndef _STDARG_H
#define _STDARG_H

// Define a va_list object here.
#if !defined(VA_LIST_DEFINED)
#define VA_LIST_DEFINED
typedef __builtin_va_list va_list;
#endif


/**** MACROS ****/
#if defined(__LIBK) || defined(__KERNEL__)
// VA_SIZE(TYPE) - Round up width of objects pushed on stack.
#define	VA_SIZE(TYPE)					\
	((sizeof(TYPE) + sizeof(STACKITEM) - 1)	\
		& ~(sizeof(STACKITEM) - 1))
#endif

#define va_start(x, y) __builtin_va_start(x, y)
#define va_arg(x, y) __builtin_va_arg(x, y)
#define va_end(x) __builtin_va_end(x)

#endif


_End_C_Header