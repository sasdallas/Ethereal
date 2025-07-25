/**
 * @file libpolyhedron/pthread/cleanup.c
 * @brief pthread cleanup routines
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <pthread.h>
#include <stdlib.h>

void pthread_cleanup_push(void (*routine)(void*), void *arg) {
    thread_tcb_t *tcb = __get_tcb();

    // Allocate new cleanup object
    __thread_cleanup_t *cleanup = malloc(sizeof(__thread_cleanup_t));
    assert(cleanup);

    cleanup->next = tcb->cleanups;
    cleanup->func = routine;
    cleanup->arg = arg;
    tcb->cleanups = cleanup;
}


void pthread_cleanup_pop(int execute) {
    thread_tcb_t *tcb = __get_tcb();

    // Pop
    __thread_cleanup_t *cleanup = tcb->cleanups;
    tcb->cleanups = cleanup->next;

    if (execute) cleanup->func(cleanup->arg);

    free(cleanup);
}