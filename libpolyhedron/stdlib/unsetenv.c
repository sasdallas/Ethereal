/**
 * @file libpolyhedron/stdlib/unsetenv.c
 * @brief unsetenv
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <stdlib.h>
#include <string.h>


extern char **environ;
extern int envc;

int unsetenv(const char *name) {
    // Find the element
    int entry = -1;
    size_t l = strlen(name);
    for (int i = 0; environ[i]; i++) {
        if (entry < 0 && strstr(environ[i], name) == environ[i] && environ[i][l] == '=') {
            // We have a match!
            entry = i;
            break;
        }
    }

    // So we'll replace the last entry of environ with it
    if (entry == -1) return 0;

    // Find the last entry of environ
    int last = 0;
    for (; environ[last]; last++);

    // Swap
    free(environ[entry]);
    environ[entry] = environ[last];
    environ[last] = NULL;
    envc--;

    return 0;
}