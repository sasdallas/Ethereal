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
#include <sys/ethereal/auxv.h>
#include <sys/ethereal/thread.h>
#include <sys/mman.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <stdio.h>

/* auxv */
extern __auxv_t *__get_auxv();

/* Initial pthread startup function */
struct __pthread_startup_context {
    void *(*entry)(void *);
    void *argument;
    pthread_t *thr;
};

__attribute__((naked)) void *__pthread_startup(void *arg) {
    // TODO: Stop leaking ctx
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

    // Then, create a TLS for the new thread
    void *tls = mmap(NULL, 8192, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (tls == MAP_FAILED) {
        munmap(stk, stack_size);
        return -1;
    }

    // Zero TLS
    memset(tls, 0, 8192);

    // Copy initial TLS data
    __auxv_t *auxv = __get_auxv();
    if (auxv && auxv->tls) {
        memcpy((void*)tls + 4096 - auxv->tls_size, (void*)auxv->tls, auxv->tls_size);
    }

    // Create TCB structure
    thread_tcb_t *tcb = (thread_tcb_t*)(tls + 4096);
    tcb->self = tcb;
    tcb->_errno = 0;
    tcb->dtv[0] = __get_tcb()->dtv[0];
    for (unsigned int i = 1; i <= tcb->dtv[0]; i++) {
        tcb->dtv[i] = __get_tcb()->dtv[i] - (uintptr_t)__get_tcb() + (uintptr_t)tcb; 
    } 


    // Make startup context
    // TODO: Not stack allocated?
    struct __pthread_startup_context *ctx = malloc(sizeof(struct __pthread_startup_context));
    ctx->entry = func;
    ctx->argument = arg;
    ctx->thr = thread;

    // Now make a new thread
    pid_t tid = ethereal_createThread((uintptr_t)stk + stack_size, (uintptr_t)tcb, __pthread_startup, ctx);
    if (tid >= 0) *thread = (pthread_t)tid;
    return (tid >= 0) ? 0 : -1;
}

__attribute__((noreturn)) void pthread_exit(void *retval) {
    ethereal_exitThread(retval);
}

int pthread_join(pthread_t thr, void **retval) {
    return ethereal_joinThread(thr, retval);
}

pthread_t pthread_self() {
    return (pthread_t)ethereal_gettid();
}

int pthread_getschedparam(pthread_t pthread, int *__restrict policy, const struct sched_param *__restrict param) {
    fprintf(stderr, "pthread_getschedparam: stub\n");
    return ENOTSUP;
}

int pthread_setschedparam(pthread_t pthread, int policy, const struct sched_param *param) {
    fprintf(stderr, "pthread_setschedparam: stub\n");
    return ENOTSUP;
}

int pthread_detach(pthread_t pthread) {
    fprintf(stderr, "pthread_detach: stub\n");
    return ENOTSUP;
}

int pthread_sigmask(int how, const sigset_t *set, sigset_t *oldset) {
    int r = sigprocmask(how, set, oldset);
    if (r < 0) return errno;
    return 0;
}