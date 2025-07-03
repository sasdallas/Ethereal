/**
 * @file libpolyhedron/stdlib/getenv.c
 * @brief getenv
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>

extern char **environ;

char *getenv(const char *name) {
    char **envp = environ;
    size_t namelen = strlen(name);
    if (!namelen) return NULL;
    while (*envp) {
        // Does the key environment variable contain name?
        char *key = strstr(*envp, name);
        if (key == *envp) {
            // Ok, it's there, but what about the equal sign?
            if (key[namelen] == '=') return &((*envp)[namelen+1]);
        }
        envp++;
    }

    return NULL;
}