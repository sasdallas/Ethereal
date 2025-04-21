/**
 * @file libpolyhedron/stdlib/malloc.c
 * @brief malloc, calloc, realloc, etc.
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

#if defined(__LIBK)

// Forwarders to kernel functions
#include <kernel/mem/alloc.h>
#include <kernel/debug.h>


__attribute__((malloc)) void *malloc( size_t size ) {
    return kmalloc(size);
}

__attribute__((malloc)) void *calloc( size_t num, size_t size ) {
    return kcalloc(num, size);
}

__attribute__((malloc)) void *realloc( void *ptr, size_t new_size ) {
    return krealloc(ptr, new_size);
}

void free( void *ptr ) {
    return kfree(ptr);
}

#endif