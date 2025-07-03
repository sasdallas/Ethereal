/**
 * @file libpolyhedron/pwd/pwent.c
 * @brief getpwent, setpwent, endpwent
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
#include <stdio.h>

static FILE *__pwent = NULL;

struct passwd *getpwent() {
    if (!__pwent) {
        __pwent = fopen("/etc/passwd", "r");
    }

    return fgetpwent(__pwent);
}

void setpwent() {
    if (__pwent) {
        rewind(__pwent);
    }
}

void endpwent() {
    if (__pwent) {
        fclose(__pwent);
        __pwent = NULL;
    }
}