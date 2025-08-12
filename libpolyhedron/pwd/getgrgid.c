/**
 * @file libpolyhedron/pwd/getgrgid.c
 * @brief getgrgid
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <pwd.h>
#include <grp.h>
#include <errno.h>

// TODO: Stub for building vim

struct group *getgrgid(gid_t gid)  {
    errno = ESRCH;
    return NULL;
}