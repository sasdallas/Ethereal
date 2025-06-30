/**
 * @file libpolyhedron/include/inttypes.h
 * @brief inttypes
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

#ifndef _INTTYPES_H
#define _INTTYPES_H

/**** INCLUDES ****/
#include <stdint.h>

/**** DEFINITIONS ****/

/* Integer format specifiers */
#define PRIi8           "i"
#define PRIi16          "i"
#define PRIi32          "i"
#define PRIi64          "li"

/* Hex format specifiers */
#define PRIx8           "x"
#define PRIx16          "x"
#define PRIx32          "x"
#define PRIx64          "lx"

/* Unsigned format specifiers */
#define PRIu8           "u"
#define PRIu16          "u"
#define PRIu32          "u"
#define PRIu64          "lu"

/* Decimal format specifiers */
#define PRId8           "d"
#define PRId16          "d"
#define PRId32          "d"
#define PRId64          "lds"

/* 'o' format */
#define PRIo8     "hho"
#define PRIo16    "ho"
#define PRIo32    "lo"
#define PRIo64    "llo"

/**** FUNCTIONS ****/

intmax_t strtoimax(const char *, char **, int base);
uintmax_t strtoumax(const char *, char **, int base);

#endif

_End_C_Header