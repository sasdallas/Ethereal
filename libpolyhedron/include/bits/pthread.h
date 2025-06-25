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
#include <stdatomic.h>
#include <stddef.h>

/**** TYPES ****/

typedef struct __pthread {
    unsigned int tid                // Thread ID
} pthread_t;

/* ATTRIBUTES */

typedef struct __pthread_attr {
    size_t stack_size;              // By default PTHREAD_STACK_SIZE
    int temp;                       // TODO
} pthread_attr_t;

typedef struct __pthread_rwlockattr {
    int temp;                       // TODO
} pthread_rwlockattr_t;

typedef struct __pthread_mutexattr {
    int type;                       // Type
    int pshared;                    // Process shared
    int kind;                       // Kind
} pthread_mutexattr_t;

/* LOCKS */

typedef atomic_flag pthread_spinlock_t;

typedef struct __pthread_rwlock {
    pthread_rwlockattr_t attr;      // Attribute
    pthread_spinlock_t lock;        // Actual lock
    unsigned long writers;          // Writers holding the lock
} pthread_rwlock_t;

typedef struct __pthread_mutex {
    pthread_mutexattr_t attr;       // Attribute
    pthread_spinlock_t lock;        // Lock
} pthread_mutex_t;

/* OTHER */

typedef int pthread_once_t;
typedef unsigned long pthread_key_t;

#endif

_End_C_Header