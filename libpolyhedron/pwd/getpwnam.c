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

struct passwd *getpwname(const char *name) {
    // STUB UNTIL FORMAT IS DECIDED
    struct passwd *ent = malloc(sizeof(struct passwd));
    ent->pw_name = (char*)name;
    ent->pw_passwd = "";
    ent->pw_gid = ent->pw_uid = 0;
    ent->pw_shell = "/usr/bin/essence";
    ent->pw_gecos = (char*)name;
    ent->pw_dir = "/";

    return ent;
}

struct passwd *getpwuid(uid_t uid) {
    // STUB UNTIL FORMAT IS DECIDED
    struct passwd *ent = malloc(sizeof(struct passwd));
    ent->pw_name = "user";
    ent->pw_passwd = "";
    ent->pw_gid = ent->pw_uid = uid;
    ent->pw_shell = "/usr/bin/essence";
    ent->pw_gecos = "user";
    ent->pw_dir = "/";

    return ent;
}