/**
 * @file libpolyhedron/libgen/basename.c
 * @brief basename
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <libgen.h>
#include <string.h>

char *basename(char *path) {
    if (!path || !(*path)) return ".";
    if (!strcmp(path, "/")) return "/";

    char *slash = strchr(path, '/');
    char *ret = slash;
    while (slash) {
        slash++;
        if (!(*slash)) {
            break;
        }

        // Update ret
        ret = slash;

        // next?
        slash = strchr(slash, '/');
    }

    return ret;
}