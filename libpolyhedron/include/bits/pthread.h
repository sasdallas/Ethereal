/**
 * @file libpolyhedron/include/bits/pthread.h
 * @brief pthread
 * 
 * Unfinalized
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

#ifndef _BITS_PTHREAD_H
#define _BITS_PTHREAD_H

/**** INCLUDES ****/
#include <stddef.h>
#include <stdint.h>

/**** TYPES ****/

typedef unsigned long pthread_t;

typedef uintptr_t __dtv;

typedef struct __thread_tcb {
    struct __thread_tcb     *self;      // Self pointer for TLS
    int _errno;                         // Errno for this thread

    __dtv                   dtv[];      // dtv array
} __attribute__((packed)) thread_tcb_t;

/* ATTRIBUTES */

typedef struct __pthread_attr {
    size_t stack_size;              // By default PTHREAD_STACK_SIZE
    unsigned char sched;            // Scheduler param
    int temp;                       // TODO
} pthread_attr_t;

typedef struct __pthread_rwlockattr {
    int temp;                       // TODO
} pthread_rwlockattr_t;

typedef struct __pthread_mutexattr {
    unsigned char type;             // Type
    unsigned char pshared;          // Process shared
    unsigned char protocol;         // Protocol
    unsigned char robust;           // Robust
} pthread_mutexattr_t;

typedef struct __pthread_condattr {
    int clock;                      // Clock ID
    unsigned char shared;           // Shared
} pthread_condattr_t;

/* LOCKS */

typedef volatile int pthread_spinlock_t;

typedef struct __pthread_rwlock {
    pthread_rwlockattr_t attr;      // Attribute
    pthread_spinlock_t lock;        // Actual lock
    unsigned long writers;          // Writers holding the lock
} pthread_rwlock_t;

typedef struct __pthread_mutex {
    pthread_mutexattr_t attr;       // Attribute
    pthread_spinlock_t lock;        // Lock
} pthread_mutex_t;

/* Thank you Bananymous :D */
typedef struct __pthread_condblocker {
    struct __pthread_condblocker *next;
    struct __pthread_condblocker *prev;
    unsigned char signalled;
} __pthread_condblocker_t;

typedef struct __pthread_cond {
    pthread_condattr_t attr;        // Attribute
    pthread_spinlock_t lock;        // Lock
    __pthread_condblocker_t *blk;   // Blocker linked list (thank you Banan for the idea)
} pthread_cond_t;

/* OTHER */

typedef int pthread_once_t;
typedef unsigned long pthread_key_t;

#endif

_End_C_Header