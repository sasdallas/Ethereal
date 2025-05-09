/**
 * @file libpolyhedron/include/assert.h
 * @brief assert
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

#ifndef ASSERT_H
#define ASSERT_H

#undef assert

void __assert_failed(const char *file, int line, const char *stmt);
#define assert(statement) (statement) ? (void)0 : __assert_failed(__FILE__, __LINE__, #statement)

#endif

_End_C_Header