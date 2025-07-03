/**
 * @file libpolyhedron/include/grp.h
 * @brief group
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

#ifndef _GRP_H
#define _GRP_H

/**** INCLUDES ****/
#include <sys/types.h>

/**** TYPES ****/

struct group {
    char *gr_name;          // The name of the group
    gid_t gr_gid;           // ID of the group
    char **gr_mem;          // Member names (null-terminated)
};

/**** FUNCTIONS ****/

struct group  *getgrgid(gid_t);
struct group  *getgrnam(const char *);
int            getgrgid_r(gid_t, struct group *, char *,
                   size_t, struct group **);
int            getgrnam_r(const char *, struct group *, char *,
                   size_t , struct group **);
struct group  *getgrent(void);
void           endgrent(void);
void           setgrent(void);

#endif

_End_C_Header