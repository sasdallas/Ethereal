/**
 * @file hexahedron/klib/stdlib/malloc.c
 * @brief malloc + friends (thunks to kmalloc)
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
#include <kernel/mm/alloc.h>

void *malloc(size_t size) {
    return kmalloc(size);
}

void free(void *p) {
    return kfree(p);
}

void *calloc(size_t n, size_t size) {
    return kcalloc(n, size);
}

void *realloc(void *p, size_t size) {
    return krealloc(p, size);
}