/**
 * @file libpolyhedron/include/stdlib.h
 * @brief Standard library header file
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

#ifndef _STDLIB_H
#define _STDLIB_H

/**** INCLUDES ****/
#include <stdint.h>
#include <stddef.h>
#include <assert.h>

/**** DEFINITIONS ****/

#define EXIT_SUCCESS    0
#define EXIT_FAILURE    1

#define RAND_MAX        32767
#define PATH_MAX        512     // !!!: This should be in limits.h
#define MB_CUR_MAX      ((size_t)4)

/**** TYPES ****/

/* div structures */
#define __define_div_structure(n, t) typedef struct { t quot; t rem; } n;

__define_div_structure(div_t, int);
__define_div_structure(ldiv_t, long);
__define_div_structure(lldiv_t, long long);

#undef __define_div_structure

/**** FUNCTIONS ****/

__attribute__((__noreturn__)) void abort(void);
__attribute__((__noreturn__)) void exit(int status);
int abs(int x);
long labs(long j);
long long llabs(long long j);
int atoi(const char*);
double atof(const char *nptr);
long atol(const char *nptr);
unsigned long long int strtoull(const char*, char**, int);
unsigned long int strtoul(const char*, char**, int);
long int strtol(const char*, char**, int);
long long int strtoll(const char*, char**, int);
double strtod(const char*, char**);
float strtof(const char *nptr, char **endptr);
long double strtold(const char *str, char **endptr);
div_t div(int x, int y);
ldiv_t ldiv(long x, long y);
lldiv_t lldiv(long long x, long long y);

char *mktemp(char *);

void *bsearch(const void *key, const void *base, size_t nel, size_t width, int (*compar)(const void *, const void *));

char *getenv(const char *name);
int putenv(char *string);
int setenv(const char *name, const char *value, int overwrite);
int unsetenv(const char *name);

__attribute__((malloc)) void *malloc( size_t size );
__attribute__((malloc)) void *calloc( size_t num, size_t size );
__attribute__((malloc)) void *realloc( void *ptr, size_t new_size );
void free( void *ptr );

int rand(void);
void srand(unsigned int seed);

void qsort(void * base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));
void qsort_r(void * base, size_t nmemb, size_t size, int (*compar)(const void *, const void *, void *), void * arg);

int system(const char *command);

void __cxa_finalize(void *d);
int __cxa_atexit(void (*func) (void *), void * arg, void * dso_handle);
int atexit(void (*func)());

char *realpath(const char *path, char *resolved_path);

int mbtowc(wchar_t *pwcs,const char *str,size_t n);
size_t mbstowcs(wchar_t *pwcs,const char *str,size_t n);
int mblen(const char *s, size_t n);



#endif

_End_C_Header
