/**
 * @file libpolyhedron/libc/elf.c
 * @brief Tiny little ELF parser to initialize sections such as PT_TLS
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
#include <stdint.h>
#include <pthread.h>
#include <string.h>
#include <sys/ethereal/auxv.h>

extern __auxv_t *__get_auxv();

void __elf_load_tls() {
    __auxv_t *auxv = __get_auxv();

    if (auxv->tls) {
        // We have to copy the TLS into dtv
        thread_tcb_t *tcb = __get_tcb();

        memcpy((void*)tcb->dtv[1] + 4096 - auxv->tls_size, (void*)auxv->tls, auxv->tls_size);
    }
}