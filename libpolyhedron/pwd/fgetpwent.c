/**
 * @file libpolyhedron/pwd/fgetpwent.c
 * @brief fgetpwent
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <pwd.h>

static char __pwdbuf[2048];
static struct passwd __pwent; // TODO: Not static?

struct passwd *fgetpwent(FILE *stream) {
    if (!stream) return NULL;

    // Read from the stream
    __pwdbuf[0] = 0; // Just in case
    fgets(__pwdbuf, 2048, stream);
    *(strchrnul(__pwdbuf, '\n')) = 0;

    // Now let's start parsing
    char *save;
    char *pch = strtok_r(__pwdbuf, ":", &save);
    char *tokens[8] = { 0 };
    int i = 0;
    while (pch) {
        tokens[i] = pch;
        i++;
        pch = strtok_r(NULL, ":", &save);
    }

    // The token structure:
    // name:passwd:uid:gid:gecos:dir:shell
    if (i < 7) return NULL;

    __pwent.pw_name = tokens[0];
    __pwent.pw_passwd = tokens[1];
    __pwent.pw_uid = strtol(tokens[2], NULL, 10);
    __pwent.pw_gid = strtol(tokens[3], NULL, 10);
    __pwent.pw_gecos = tokens[4];
    __pwent.pw_dir = tokens[5];
    __pwent.pw_shell = tokens[6];

    return &__pwent;
}