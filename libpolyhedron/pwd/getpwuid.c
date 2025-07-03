/**
 * @file libpolyhedron/pwd/getpwuid.c
 * @brief getpwuid
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

struct passwd *getpwuid(uid_t uid) {
    setpwent();

    struct passwd *p = getpwent();
    while (p) {
        if (p->pw_uid == uid) {
            return p;
        }

        p = getpwent();
    }

    return NULL;
}