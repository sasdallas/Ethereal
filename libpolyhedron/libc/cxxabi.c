/**
 * @file libpolyhedron/libc/cxxabi.c
 * @brief __cxa functions
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <unistd.h>
#include <limits.h>

// !!!: limits.h build hack
#ifndef ATEXIT_MAX
#define ATEXIT_MAX      32
#endif

typedef struct __atexit_callback {
    void (*atexit)(void*);
    void *argument;
    void *dso;
} __atexit_callback_t;

/* atexit table */
static __atexit_callback_t __atexit_table[ATEXIT_MAX];
static int __atexit_index = 0;


int __cxa_atexit(void (*func)(void*), void *arg, void *dso_handle) {
    if (__atexit_index >= ATEXIT_MAX) {
        return -1; // ??? Error handling?
    }

    __atexit_table[__atexit_index].atexit = func;
    __atexit_table[__atexit_index].argument = arg;
    __atexit_table[__atexit_index].dso = dso_handle;
    __atexit_index++;

    return 0;
}

void __cxa_finalize(void * d) {
    for (int i = 0; i < __atexit_index; i++) {
        if (__atexit_table[i].dso == d) {
            __atexit_table[i].atexit(__atexit_table[i].argument);
        }
    }

    // Reset so no more calls
    __atexit_index = 0;
}