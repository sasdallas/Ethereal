/**
 * @file libpolyhedron/stdlib/setenv.c
 * @brief setenv/putenv
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

extern int envc;

int setenv(const char *name, const char *value, int overwrite) {
#ifndef __LIBK
    if (!name) {
        errno = EINVAL;
        return -1;
    }
#endif

    // Overwrite check
    int env_exists = getenv(name) ? 1 : 0;
    if (!overwrite && env_exists) return 0;

    // Construct a new string
    char *string_to_use = malloc(strlen(name) + strlen(value) + 1);
    sprintf(string_to_use, "%s=%s", name, value);
    return putenv(string_to_use);
}

int putenv(char *string) {
    // We need to convert string into a name and value
#ifndef __LIBK
    if (!string) {
        errno = EINVAL;
        return -1;
    }
#endif


    // Construct a new string
    char tmp_string[strlen(string)+1];
    strcpy(tmp_string, string);

    // Find the equal sign
    char *equal_ptr = strchr(tmp_string, '=');
    if (!equal_ptr) return -EINVAL;

    // Get name
    char *name = string;
    *equal_ptr = 0;

    // Now we have a name and a value. Let's search environ
    char **envp = environ;
    while (*envp) {
        if (strstr(*envp, name) && (*envp)[strlen(name)] == '=') {
            *envp = strdup(string); // !!!: Is this good?
            return 0;
        }
        envp++;
    }

    // Nothing. Let's add a new variable
    envc++;
    environ = realloc(environ, sizeof(char*) * envc);
    environ[envc-1] = strdup(string);
    environ[envc] = NULL;
    return 0;
}
