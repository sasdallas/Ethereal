/**
 * @file libpolyhedron/stdlib/mktemp.c
 * @brief mktemp
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
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>


/**
 * The POSIX standard says: Never use mktemp(). Some implementations follow 4.3BSD and
 *                          replace XXXXXX by the current process ID and a single letter, so
 *                          that at most 26 different names can be returned.
 * 
 * Well, guess which implementation Ethereal uses?
 */

char letter = 'a';

char *mktemp(char *template) {
    // The last 6 characters of template must be XXXXXX
    size_t len = strlen(template);
    if (len < 6) {

#ifndef __LIBK
        errno = EINVAL;
#endif

        return NULL;
    }

    char *last = template + len - 6;
    if (strncmp(last, "XXXXXX", 6)) {
#ifndef __LIBK
        errno = EINVAL;
#endif

        return NULL;
    }

    snprintf(last, 6, "%05d%c", getpid(), letter++);
    return template;
}