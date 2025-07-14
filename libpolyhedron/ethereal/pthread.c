/**
 * @file libpolyhedron/ethereal/pthread.c
 * @brief Ethereal pthread API
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <sys/syscall.h>
#include <sys/ethereal/thread.h>
#include <pthread.h>
#include <errno.h>

DEFINE_SYSCALL4(create_thread, SYS_CREATE_THREAD, uintptr_t, uintptr_t, void *, void *);
DEFINE_SYSCALL0(gettid, SYS_GETTID);
DEFINE_SYSCALL1(settls, SYS_SETTLS, uintptr_t);
DEFINE_SYSCALL1(exit_thread, SYS_EXIT_THREAD, void *);
DEFINE_SYSCALL2(join_thread, SYS_JOIN_THREAD, pid_t, void **);
DEFINE_SYSCALL2(kill_thread, SYS_KILL_THREAD, pid_t, int);

pid_t ethereal_createThread(uintptr_t stack, uintptr_t tls, void *(*func)(void *), void *arg) {
    __sets_errno(__syscall_create_thread(stack, tls, func, arg));
}

pid_t ethereal_gettid() {
    return __syscall_gettid();
}

int ethereal_settls(uintptr_t tls) {
    __sets_errno(__syscall_settls(tls));
}

__attribute__((noreturn)) void ethereal_exitThread(void *retval) {
    __syscall_exit_thread(retval);
    __builtin_unreachable();
}

int ethereal_joinThread(pid_t tid, void **retval) {
    __sets_errno(__syscall_join_thread(tid, retval));
}

int ethereal_killThread(pid_t tid, int sig) {
    __sets_errno(__syscall_kill_thread(tid, sig));
}