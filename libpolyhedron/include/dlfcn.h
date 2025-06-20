/**
 * @file libpolyhedron/include/dlfcn.h
 * @brief dlfcn
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

#ifndef _DLFCN_H
#define _DLFCN_H

/**** DEFINITIONS ****/
#define RTLD_NOW        1

/**** FUNCTIONS ****/

void  *dlopen(const char *filename, int flags);
void  *dlsym(void *handle, const char *symbol);
int    dlclose(void *handle);
char  *dlerror(void);

#endif

_End_C_Header;