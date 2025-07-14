/**
 * @file libpolyhedron/pthread/tls.c
 * @brief TLS functions
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <sys/ethereal/thread.h>
#include <sys/mman.h>
#include <string.h>

void __tls_init() {
    // Get TLS
    void *tls = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    memset(tls, 0, 4096);

    // Self-pointer
    char **self = (char**)tls;
    *self = (char*)self;

    ethereal_settls((uintptr_t)tls);
}