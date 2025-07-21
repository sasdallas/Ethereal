/**
 * @file libpolyhedron/include/nl_types.h
 * @brief data types
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

#ifndef _NL_TYPES_H
#define _NL_TYPES_H

/**** TYPES ****/

typedef void *nl_catd;
typedef int nl_item;

/**** DEFINITIONS ****/

#define NL_SETD                 1
#define NL_CAT_LOCALE           1

/**** FUNCTIONS ****/

int       catclose(nl_catd);
char     *catgets(nl_catd, int, int, const char *);
nl_catd   catopen(const char *, int);

#endif

_End_C_Header