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
#include <stdlib.h>


/**
 * libpolyhedron uses TCB structures to hold thread error numbers and more
 * Each TCB structure contains a self pointer for TLS not using __tls_get_addr
 */
thread_tcb_t *__get_tcb() {
#ifdef __ARCH_X86_64__
    thread_tcb_t *tcb;
    asm volatile ("movq %%fs:0, %0" : "=r"(tcb));
    return tcb;
#elif defined(__ARCH_I386__)
    thread_tcb_t *tcb;
    asm volatile ("movl %%gs:0, %0" : "=r"(tcb));
    return tcb;
#elif defined(__ARCH_AARCH64__)
    return NULL; // TODO
#else
    #error "__get_tcb is required"
#endif
}

/**
 * TLS initialize function for the main thread
 */
void __tls_init() {
    // Get TLS
    void *tls = mmap(NULL, 8192, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    memset(tls, 0, 8192);

    // Set self pointer in TCB
    thread_tcb_t *tcb = (thread_tcb_t*)((uintptr_t)tls + 4096);
    tcb->self = tcb;

    // Initialize other parts of TCB
    tcb->_errno = 0;
    
    // Setup dtv array
    tcb->dtv[0] = 1;
    tcb->dtv[1] = (uintptr_t)tls;

    // Set TLS
    ethereal_settls((uintptr_t)tcb);
}

void *__tls_get_addr(void *input) {
#ifdef __ARCH_X86_64__
    struct __tls_index {
        uintptr_t ti_module;
        uintptr_t ti_offset;
    };

    struct __tls_index *ti = (struct __tls_index*)input;
    thread_tcb_t *tcb = __get_tcb();

    return (void*)(tcb->dtv[ti->ti_module] + ti->ti_offset);
#elif defined(__ARCH_I386__)
    return NULL; // TODO
#elif defined(__ARCH_AARCH64__)
    return NULL; // TODO
#else
    #error "__tls_get_addr is required"
#endif

}