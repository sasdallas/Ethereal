/**
 * @file libpolyhedron/include/sys/ethereal/thread.h
 * @brief Ethereal threading/pthread implementation
 * 
 * Ethereal supports pthreads at the scheduler. These are system calls to interact.
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <sys/cheader.h>

_Begin_C_Header

#ifndef _SYS_ETHEREAL_THREAD_H
#define _SYS_ETHEREAL_THREAD_H

/**** INCLUDES ****/
#include <stdint.h>
#include <sys/types.h>

/**** FUNCTIONS ****/

/**
 * @brief Create a new thread
 * @param stack The stack of the thread
 * @param tls (Optional) TLS of the thread
 * @param func The entrypoint function to the thread
 * @param arg The argument to thread
 * @returns PID or -1
 */
pid_t ethereal_createThread(uintptr_t stack, uintptr_t tls, void *(*func)(void *), void *arg);

/**
 * @brief Get TID of the current thread
 * @returns TID
 */
pid_t ethereal_gettid();

/**
 * @brief Set the TLS base of the current thread
 * @param tls The TLS of the current thread
 * @returns 0 on success
 */
int ethereal_settls(uintptr_t tls);

/**
 * @brief Exit the current thread
 * @param retval The return value to let other threads see
 */
__attribute__((noreturn)) void ethereal_exitThread(void *retval);

/**
 * @brief Join and wait for a thread to complete
 * @param tid The ID of the thread
 * @param retval Pointer to return value, if you want it
 * @returns 0 on success
 */
int ethereal_joinThread(pid_t tid, void **retval);

/**
 * @brief Kill a thread
 * @param tid The ID of the thread
 * @param sig The signal to send to the thread
 * @returns 0 on success
 */
int ethereal_killThread(pid_t tid, int sig);


#endif

_End_C_Header