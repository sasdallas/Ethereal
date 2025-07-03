/**
 * @file libpolyhedron/include/limits.h
 * @brief limits.h
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

#ifndef _LIMITS_H
#define _LIMITS_H

/**** DEFINITIONS ****/
#define _POSIX_AIO_LISTIO_MAX                   2
#define _POSIX_AIO_MAX                          1
#define _POSIX_ARG_MAX                          4096
#define _POSIX_CHILD_MAX                        25
#define _POSIX_DELAYTIMER_MAX                   32
#define _POSIX_HOST_NAME_MAX                    256
#define _POSIX_LINK_MAX                         8
#define _POSIX_LOGIN_NAME_MAX                   9
#define _POSIX_MAX_CANON                        255
#define _POSIX_MAX_INPUT                        255
#define _POSIX_MQ_OPEN_MAX                      8
#define _POSIX_MQ_PRIO_MAX                      32
#define _POSIX_NAME_MAX                         14
#define _POSIX_NGROUPS_MAX                      8
#define _POSIX_OPEN_MAX                         20
#define _POSIX_PATH_MAX                         256
#define _POSIX_PIPE_BUF                         512
#define _POSIX_RE_DUP_MAX                       255
#define _POSIX_RTSIG_MAX                        8
#define _POSIX_SEM_NSEMS_MAX                    256
#define _POSIX_SEM_VALUE_MAX                    32767
#define _POSIX_SIGQUEUE_MAX                     32
#define _POSIX_SSIZE_MAX                        32767
#define _POSIX_SS_REPL_MAX                      4
#define _POSIX_STREAM_MAX                       8
#define _POSIX_SYMLINK_MAX                      255
#define _POSIX_SYMLOOP_MAX                      8
#define _POSIX_THREAD_DESTRUCTOR_ITERATIONS     4
#define _POSIX_THREAD_KEYS_MAX                  128
#define _POSIX_THREAD_THREADS_MAX               64
#define _POSIX_TIMER_MAX                        32
#define _POSIX_TRACE_EVENT_NAME_MAX             30
#define _POSIX_TRACE_NAME_MAX                   8
#define _POSIX_TRACE_SYS_MAX                    8
#define _POSIX_TRACE_USER_EVENT_MAX             32
#define _POSIX_TTY_NAME_MAX                     9
#define _POSIX_TZNAME_MAX                       6

#define _POSIX2_BC_BASE_MAX                     99
#define _POSIX2_BC_DIM_MAX                      2048
#define _POSIX2_BC_SCALE_MAX                    99
#define _POSIX2_BC_STRING_MAX                   1000
#define _POSIX2_CHARCLASS_NAME_MAX              14
#define _POSIX2_COLL_WEIGHTS_MAX                2
#define _POSIX2_EXPR_NEST_MAX                   32
#define _POSIX2_LINE_MAX                        2048
#define _POSIX2_RE_DUP_MAX                      255
#define _XOPEN_IOV_MAX                          16
#define _XOPEN_NAME_MAX                         255
#define _XOPEN_PATH_MAX                         1024

/* Integer sizes */
#define CHAR_MAX    SCHAR_MAX
#define CHAR_MIN    SCHAR_MIN
#define UCHAR_MIN   0
#define UCHAR_MAX   255

#define SCHAR_MAX   __SCHAR_MAX__
#define SHRT_MAX    __SHRT_MAX__
#define INT_MAX     __INT_MAX__
#define LONG_MAX    __LONG_MAX__
#define LLONG_MAX   __LONG_LONG_MAX__
#define SSIZE_MAX   __PTRDIFF_MAX__

#define SCHAR_MIN   (-SCHAR_MAX - 1)
#define SHRT_MIN    (-SHRT_MAX - 1)
#define INT_MIN     (-INT_MAX - 1)
#define LONG_MIN    (-LONG_MAX - 1)
#define LLONG_MIN   (-LLONG_MAX - 1)
#define SSIZE_MIN   (-SSIZE_MAX - 1)

#define USCHAR_MAX  (SCHAR_MAX * 2 + 1)
#define USHRT_MAX   (SHRT_MAX * 2 + 1)
#define UINT_MAX    (INT_MAX * 2U + 1)
#define ULONG_MAX   (LONG_MAX * 2UL + 1)
#define ULLONG_MAX  (LLONG_MAX * 2ULL + 1)

#define UINT32_MAX  ULONG_MAX
#define UINT64_MAX  ULLONG_MAX

#define INT_FAST64_MAX  LLONG_MAX

/* Pathname variable values */
#define FILESIZEBITS                32
#define LINK_MAX                    _POSIX_LINK_MAX
#define MAX_CANON                   _POSIX_MAX_CANON
#define MAX_INPUT                   _POSIX_MAX_INPUT
#define NAME_MAX                    _POSIX_NAME_MAX
#define PATH_MAX                    _POSIX_PATH_MAX
#define PIPE_BUF                    _POSIX_PIPE_BUF

/* Runtime invariant values */
#define AIO_LISTIO_MAX                  _POSIX_AIO_LISTIO_MAX
#define AIO_MAX                         _POSIX_AIO_MAX
#define AIO_PRIO_DELTA_MAX              0
#define ARG_MAX                         _POSIX_ARG_MAX
#define ATEXIT_MAX                      32
#define CHILD_MAX                       _POSIX_CHILD_MAX
#define DELAYTIMER_MAX                  _POSIX_DELAYTIMER_MAX
#define HOST_NAME_MAX                   _POSIX_HOST_NAME_MAX
#define IOV_MAX                         _XOPEN_IOV_MAX
#define LOGIN_NAME_MAX                  _POSIX_LOGIN_NAME_MAX
#define MQ_OPEN_MAX                     _POSIX_MQ_OPEN_MAX
#define MQ_PRIO_MAX                     _POSIX_MQ_PRIO_MAX
#define OPEN_MAX                        _POSIX_OPEN_MAX
#define PTHREAD_DESTRUCTOR_ITERATIONS   _POSIX_THREAD_DESTRUCTOR_ITERATIONS
#define PTHREAD_KEYS_MAX                _POSIX_THREAD_KEYS_MAX
#define PTHREAD_THREADS_MAX             _POSIX_THREAD_THREADS_MAX
#define RTSIG_MAX                       _POSIX_RTSIG_MAX
#define SEM_NSEMS_MAX                   _POSIX_SEM_NSEMS_MAX
#define SEM_VALUE_MAX                   _POSIX_SEM_VALUE_MAX
#define SIGQUEUE_MAX                    _POSIX_SIGQUEUE_MAX
#define SS_REPL_MAX                     _POSIX_SS_REPL_MAX
#define STREAM_MAX                      _POSIX_STREAM_MAX
#define SYMLOOP_MAX                     _POSIX_SYMLOOP_MAX
#define TIMER_MAX                       _POSIX_TIMER_MAX
#define TRACE_EVENT_NAME_MAX            _POSIX_TRACE_EVENT_NAME_MAX
#define TRACE_NAME_MAX                  _POSIX_TRACE_NAME_MAX
#define TRACE_SYS_MAX                   _POSIX_TRACE_SYS_MAX
#define TRACE_USER_EVENT_MAX            _POSIX_TRACE_USER_EVENT_MAX
#define TTY_NAME_MAX                    _POSIX_TTY_NAME_MAX
#define TZNAME_MAX                      _POSIX_TZNAME_MAX

/* Page size */
#ifndef PAGESIZE
#define PAGESIZE                    4096
#endif

#define PAGE_SIZE                   PAGESIZE


/* TODO: POSIX_ definitions */
/* TODO: BC values */

#endif

_End_C_Header
