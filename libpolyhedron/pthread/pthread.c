/**
 * @file libpolyhedron/pthread/pthread.c
 * @brief pthread API
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
#include <sys/syscall.h>
#include <sys/ethereal/thread.h>
#include <sys/mman.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>

/* Initial pthread startup function */
struct __pthread_startup_context {
    void *(*entry)(void *);
    void *argument;
    pthread_t *thr;
};

__attribute__((naked)) void *__pthread_startup(void *arg) {
    struct __pthread_startup_context *ctx = (struct __pthread_startup_context*)arg;
    pthread_exit(ctx->entry(ctx->argument));
    __builtin_unreachable();
}

/**** OTHER PTHREAD FUNCTIONS ****/

int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*func)(void *), void *arg) {
    // First, create a stack for the new thread
    size_t stack_size = attr ? attr->stack_size : PTHREAD_STACK_SIZE;
    void *stk = mmap(NULL, stack_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (stk == MAP_FAILED) {
        return -1;
    }

    // Zero the stack
    memset(stk, 0, stack_size);

    // Make startup context
    // TODO: Not stack allocated?
    struct __pthread_startup_context ctx = {
        .entry = func,
        .argument = arg,
        .thr = thread,
    };

    // Now make a new thread
    pid_t tid = ethereal_createThread((uintptr_t)stk + stack_size, 0x0, __pthread_startup, &ctx);
    if (tid >= 0) *thread = (pthread_t)tid;
    return (tid >= 0) ? 0 : -1;
}

__attribute__((noreturn)) void pthread_exit(void *retval) {
    ethereal_exitThread(retval);
}

int pthread_join(pthread_t thr, void **retval) {
    return ethereal_joinThread(thr, retval);
}