/**
 * @file libpolyhedron/pwd/getpwnam.c
 * @brief getpwname
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
#include <stdlib.h>
#include <string.h>

struct passwd *getpwnam(const char *name) {
    // Rewind stream
    setpwent();

    struct passwd *p = getpwent();
    while (p) {
        if (!strcmp(p->pw_name, name)) return p;
        p = getpwent();
    }

    return NULL;
}