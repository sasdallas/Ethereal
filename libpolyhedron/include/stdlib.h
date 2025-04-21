/**
 * @file libpolyhedron/include/stdlib.h
 * @brief Standard library header file
 * 
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

#ifndef _STDLIB_H
#define _STDLIB_H

/**** INCLUDES ****/
#include <stdint.h>
#include <stddef.h>
#include <assert.h>

/**** DEFINITIONS ****/

#define EXIT_SUCCESS    0
#define EXIT_FAILURE    1


/**** FUNCTIONS ****/

__attribute__((__noreturn__)) void abort(void);

int abs(int x);
int min(int a, int b);
int max(int a, int b);

int atoi(const char*);
double atof(const char *nptr);
unsigned long long int strtoull(const char*, char**, int);
unsigned long int strtoul(const char*, char**, int);
long int strtol(const char*, char**, int);
long long int strtoll(const char*, char**, int);
double strtod(const char*, char**);

char *getenv(const char *name);
int putenv(char *string);
int setenv(const char *name, const char *value, int overwrite);
int unsetenv(const char *name);

__attribute__((malloc)) void *malloc( size_t size );
__attribute__((malloc)) void *calloc( size_t num, size_t size );
__attribute__((malloc)) void *realloc( void *ptr, size_t new_size );
void free( void *ptr );

#endif

_End_C_Header