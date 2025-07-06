/**
 * @file libpolyhedron/include/pthread.h
 * @brief pthread
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

#ifndef _PTHREAD_H
#define _PTHREAD_H

/**** INCLUDES ****/
#include <sys/types.h>
#include <stddef.h>

/**** DEFINITIONS ****/

#define PTHREAD_STACK_SIZE      16384

#define PTHREAD_CANCEL_ASYNCRONOUS      1
#define PTHREAD_CANCEL_DEFERRED         0

#define PTHREAD_CANCEL_DISABLE          0
#define PTHREAD_CANCEL_ENABLE           1

#define PTHREAD_PRIO_NONE               0
#define PTHREAD_PRIO_INHERIT            1
#define PTHREAD_PRIO_PROTECT            2

#define PTHREAD_EXPLICIT_SCHED          0
#define PTHREAD_INHERIT_SCHED           1

#define PTHREAD_MUTEX_DEFAULT           0
#define PTHREAD_MUTEX_ERRORCHECK        1
#define PTHREAD_MUTEX_NORMAL            2
#define PTHREAD_MUTEX_RECURSIVE         3

#define PTHREAD_ONCE_INIT               0

#define PTHREAD_PROCESS_PRIVATE         0
#define PTHREAD_PROCESS_SHARED          1

#define PTHREAD_SPIN_INITIALIZER        (pthread_spinlock_t)0
#define PTHREAD_MUTEX_INITIALIZER       (pthread_mutex_t){ .attr = { 0 }, .lock = PTHREAD_SPIN_INITIALIZER }
#define PTHREAD_RWLOCK_INITIALIZER      (pthread_rwlock_t){ .attr = { 0 }, .lock = PTHREAD_SPIN_INITIALIZER, .writers = 0 }


/**** FUNCTIONS ****/

int   pthread_attr_destroy(pthread_attr_t *);
int   pthread_attr_getdetachstate(const pthread_attr_t *, int *);
int   pthread_attr_getguardsize(const pthread_attr_t *, size_t *);
int   pthread_attr_getinheritsched(const pthread_attr_t *, int *);
int   pthread_attr_getschedpolicy(const pthread_attr_t *, int *);
int   pthread_attr_getscope(const pthread_attr_t *, int *);
int   pthread_attr_getstackaddr(const pthread_attr_t *, void **);
int   pthread_attr_getstacksize(const pthread_attr_t *, size_t *);
int   pthread_attr_init(pthread_attr_t *);
int   pthread_attr_setdetachstate(pthread_attr_t *, int);
int   pthread_attr_setguardsize(pthread_attr_t *, size_t);
int   pthread_attr_setinheritsched(pthread_attr_t *, int);
int   pthread_attr_setschedpolicy(pthread_attr_t *, int);
int   pthread_attr_setscope(pthread_attr_t *, int);
int   pthread_attr_setstackaddr(pthread_attr_t *, void *);
int   pthread_attr_setstacksize(pthread_attr_t *, size_t);
int   pthread_cancel(pthread_t);
void  pthread_cleanup_push(void*, void *);
void  pthread_cleanup_pop(int);
// int   pthread_cond_broadcast(pthread_cond_t *);
// int   pthread_cond_destroy(pthread_cond_t *);
// int   pthread_cond_init(pthread_cond_t *, const pthread_condattr_t *);
// int   pthread_cond_signal(pthread_cond_t *);
// int   pthread_cond_timedwait(pthread_cond_t *, 
//           pthread_mutex_t *, const struct timespec *);
// int   pthread_cond_wait(pthread_cond_t *, pthread_mutex_t *);
// int   pthread_condattr_destroy(pthread_condattr_t *);
// int   pthread_condattr_getpshared(const pthread_condattr_t *, int *);
// int   pthread_condattr_init(pthread_condattr_t *);
// int   pthread_condattr_setpshared(pthread_condattr_t *, int);
int   pthread_create(pthread_t *, const pthread_attr_t *,
          void *(*)(void *), void *);
int   pthread_detach(pthread_t);
int   pthread_equal(pthread_t, pthread_t);
void  pthread_exit(void *);
int   pthread_getconcurrency(void);
void *pthread_getspecific(pthread_key_t);
int   pthread_join(pthread_t, void **);
int   pthread_key_create(pthread_key_t *, void (*)(void *));
int   pthread_key_delete(pthread_key_t);
int   pthread_mutex_destroy(pthread_mutex_t *);
int   pthread_mutex_getprioceiling(const pthread_mutex_t *, int *);
int   pthread_mutex_init(pthread_mutex_t *, const pthread_mutexattr_t *);
int   pthread_mutex_lock(pthread_mutex_t *);
int   pthread_mutex_setprioceiling(pthread_mutex_t *, int, int *);
int   pthread_mutex_trylock(pthread_mutex_t *);
int   pthread_mutex_unlock(pthread_mutex_t *);
int   pthread_mutexattr_destroy(pthread_mutexattr_t *);
int   pthread_mutexattr_getprioceiling(const pthread_mutexattr_t *,
          int *);
int   pthread_mutexattr_getprotocol(const pthread_mutexattr_t *, int *);
int   pthread_mutexattr_getpshared(const pthread_mutexattr_t *, int *);
int   pthread_mutexattr_gettype(const pthread_mutexattr_t *, int *);
int   pthread_mutexattr_init(pthread_mutexattr_t *);
int   pthread_mutexattr_setprioceiling(pthread_mutexattr_t *, int);
int   pthread_mutexattr_setprotocol(pthread_mutexattr_t *, int);
int   pthread_mutexattr_setpshared(pthread_mutexattr_t *, int);
int   pthread_mutexattr_settype(pthread_mutexattr_t *, int);
int   pthread_once(pthread_once_t *, void (*)(void));
int   pthread_rwlock_destroy(pthread_rwlock_t *);
int   pthread_rwlock_init(pthread_rwlock_t *,
          const pthread_rwlockattr_t *);
int   pthread_rwlock_rdlock(pthread_rwlock_t *);
int   pthread_rwlock_tryrdlock(pthread_rwlock_t *);
int   pthread_rwlock_trywrlock(pthread_rwlock_t *);
int   pthread_rwlock_unlock(pthread_rwlock_t *);
int   pthread_rwlock_wrlock(pthread_rwlock_t *);
int   pthread_rwlockattr_destroy(pthread_rwlockattr_t *);
int   pthread_rwlockattr_getpshared(const pthread_rwlockattr_t *,
          int *);
int   pthread_rwlockattr_init(pthread_rwlockattr_t *);
int   pthread_rwlockattr_setpshared(pthread_rwlockattr_t *, int);
pthread_t pthread_self(void);
int   pthread_setcancelstate(int, int *);
int   pthread_setcanceltype(int, int *);
int   pthread_setconcurrency(int);
int   pthread_setspecific(pthread_key_t, const void *);
void  pthread_testcancel(void);

#endif

_End_C_Header