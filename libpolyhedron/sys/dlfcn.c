/**
 * @file libpolyhedron/sys/dlfcn.c
 * @brief dlfcn stubs
 * 
 * Technically not a sys/ file
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <dlfcn.h>
#include <stdlib.h>
#include <stdio.h>

void *dlopen(const char *filename, int flags) {
    fprintf(stderr, "dlopen: %s\n", filename);
    abort();
}

void *dlsym(void *handle, const char *symbol) {
    fprintf(stderr, "dlsym: stub\n");
    abort();
}

int dlclose(void *handle) {
    fprintf(stderr, "dlclose: stub\n");
    abort();
}

char *dlerror(void) {
    return "STUB";
}