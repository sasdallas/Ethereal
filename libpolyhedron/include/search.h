/**
 * @file libpolyhedron/include/search.h
 * @brief search.h
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

#ifndef _SYS_CHEADER_H
#define _SYS_CHEADER_H

/**** INCLUDES ****/
#include <stddef.h>

/**** TYPES ****/

typedef struct entry {
    char *key;
    void *data;
} ENTRY;

typedef enum { FIND, ENTER } ACTION;
typedef enum { preorder, postorder, endorder, leaf } VISIT;

/**** FUNCTIONS ****/

int    hcreate(size_t);
void   hdestroy(void);
ENTRY *hsearch(ENTRY, ACTION);
void   insque(void *, void *);
void  *lfind(const void *, const void *, size_t *,
          size_t, int (*)(const void *, const void *));
void  *lsearch(const void *, void *, size_t *,
          size_t, int (*)(const void *, const void *));
void   remque(void *);
void  *tdelete(const void *restrict, void **,
          int(*)(const void *, const void *));
void  *tfind(const void *, void *const *,
          int(*)(const void *, const void *));
void  *tsearch(const void *, void **,
          int(*)(const void *, const void *));
void   twalk(const void *,
          void (*)(const void *, VISIT, int ));


#endif

_End_C_Header