/**
 * @file libpolyhedron/stdlib/atexit.c
 * @brief __cxa_atexit and atexit
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

int __cxa_atexit(void (*func)(void*), void *arg, void *dso_handle) {
    // TODO
    return 0;
}

int atexit(void (*func)(void)) {
    return __cxa_atexit((void*)func, NULL, NULL);
}