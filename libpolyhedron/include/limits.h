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
#define _POSIX2_BC#define _SCALE_MAX                    99
#define _POSIX2_BC_STRING_MAX                   1000
#define _POSIX2_CHARCLASS_NAME_MAX              14
#define _POSIX2_COLL_WEIGHTS_MAX                2
#define _POSIX2_EXPR_NEST_MAX                   32
#define _POSIX2_LINE_MAX                        2048
#define _POSIX2_RE_DUP_MAX                      255
#define _XOPEN_IOV_MAX                          16
#define _XOPEN_NAME_MAX                         255
#define _XOPEN_PATH_MAX                         1024

#define _PC_2_SYMLINKS                          0
#define _PC_ALLOC_SIZE_MIN                      1
#define _PC_ASYNC_IO                            2
#define _PC_CHOWN_RESTRICTED                    3
#define _PC_FALLOC                              4
#define _PC_FILESIZEBITS                        5
#define _PC_LINK_MAX                            6
#define _PC_MAX_CANON                           7
#define _PC_MAX_INPUT                           8
#define _PC_NAME_MAX                            9
#define _PC_NO_TRUNC                            10
#define _PC_PATH_MAX                            11
#define _PC_PIPE_BUF                            12
#define _PC_PRIO_IO                             13
#define _PC_REC_INCR_XFER_SIZE                  14
#define _PC_REC_MAX_XFER_SIZE                   15
#define _PC_REC_MIN_XFER_SIZE                   16
#define _PC_REC_XFER_ALIGN                      17
#define _PC_SYMLINK_MAX                         18
#define _PC_SYNC_IO                             19
#define _PC_TEXTDOMAIN_MAX                      20
#define _PC_TIMESTAMP_RESOLUTION                21
#define _PC_VDISABLE                            22


#define _SC_2_C_BIND                            0
#define _SC_2_C_DEV                             1
#define _SC_2_CHAR_TERM                         2
#define _SC_2_FORT_RUN                          3
#define _SC_2_LOCALEDEF                         4
#define _SC_2_SW_DEV                            5
#define _SC_2_UPE                               6
#define _SC_2_VERSION                           7
#define _SC_ADVISORY_INFO                       8
#define _SC_AIO_LISTIO_MAX                      9
#define _SC_AIO_MAX                             10
#define _SC_AIO_PRIO_DELTA_MAX                  11
#define _SC_ARG_MAX                             12
#define _SC_ASYNCHRONOUS_IO                     13
#define _SC_ATEXIT_MAX                          14
#define _SC_BARRIERS                            15
#define _SC_BC_BASE_MAX                         16
#define _SC_BC_DIM_MAX                          17
#define _SC_BC_SCALE_MAX                        18
#define _SC_BC_STRING_MAX                       19
#define _SC_CHILD_MAX                           20
#define _SC_CLK_TCK                             21
#define _SC_CLOCK_SELECTION                     22
#define _SC_COLL_WEIGHTS_MAX                    23
#define _SC_CPUTIME                             24
#define _SC_DELAYTIMER_MAX                      25
#define _SC_DEVICE_CONTROL                      26
#define _SC_EXPR_NEST_MAX                       27
#define _SC_FSYNC                               28
#define _SC_GETGR_R_SIZE_MAX                    29
#define _SC_GETPW_R_SIZE_MAX                    30
#define _SC_HOST_NAME_MAX                       31
#define _SC_IOV_MAX                             32
#define _SC_IPV6                                33
#define _SC_JOB_CONTROL                         34
#define _SC_LINE_MAX                            35
#define _SC_LOGIN_NAME_MAX                      36
#define _SC_MAPPED_FILES                        37
#define _SC_MEMLOCK                             38
#define _SC_MEMLOCK_RANGE                       39
#define _SC_MEMORY_PROTECTION                   40  
#define _SC_MESSAGE_PASSING                     41
#define _SC_MONOTONIC_CLOCK                     42
#define _SC_MQ_OPEN_MAX                         43
#define _SC_MQ_PRIO_MAX                         44
#define _SC_NGROUPS_MAX                         45
#define _SC_NPROCESSORS_CONF                    46
#define _SC_NPROCESSORS_ONLN                    47
#define _SC_NSIG                                48
#define _SC_OPEN_MAX                            49
#define _SC_PAGE_SIZE                           50
#define _SC_PAGESIZE                            51
#define _SC_PRIORITIZED_IO                      52
#define _SC_PRIORITY_SCHEDULING                 53
#define _SC_RAW_SOCKETS                         54
#define _SC_RE_DUP_MAX                          55
#define _SC_READER_WRITER_LOCKS                 56
#define _SC_REALTIME_SIGNALS                    57
#define _SC_REGEXP                              58
#define _SC_RTSIG_MAX                           59
#define _SC_SAVED_IDS                           60
#define _SC_SEM_NSEMS_MAX                       61
#define _SC_SEM_VALUE_MAX                       62
#define _SC_SEMAPHORES                          63
#define _SC_SHARED_MEMORY_OBJECTS               64
#define _SC_SHELL                               65
#define _SC_SIGQUEUE_MAX                        66
#define _SC_SPAWN                               67
#define _SC_SPIN_LOCKS                          68
#define _SC_SPORADIC_SERVER                     69
#define _SC_SS_REPL_MAX                         70
#define _SC_STREAM_MAX                          71
#define _SC_SYMLOOP_MAX                         72
#define _SC_SYNCHRONIZED_IO                     73
#define _SC_THREAD_ATTR_STACKADDR               74
#define _SC_THREAD_ATTR_STACKSIZE               75
#define _SC_THREAD_CPUTIME                      76
#define _SC_THREAD_DESTRUCTOR_ITERATIONS        77
#define _SC_THREAD_KEYS_MAX                     78
#define _SC_THREAD_PRIO_INHERIT                 79
#define _SC_THREAD_PRIO_PROTECT                 80
#define _SC_THREAD_PRIORITY_SCHEDULING          81
#define _SC_THREAD_PROCESS_SHARED               82
#define _SC_THREAD_ROBUST_PRIO_INHERIT          83
#define _SC_THREAD_ROBUST_PRIO_PROTECT          84
#define _SC_THREAD_SAFE_FUNCTIONS               85
#define _SC_THREAD_SPORADIC_SERVER              86
#define _SC_THREAD_STACK_MIN                    87
#define _SC_THREAD_THREADS_MAX                  88
#define _SC_THREADS                             89
#define _SC_TIMEOUTS                            90
#define _SC_TIMER_MAX                           91
#define _SC_TIMERS                              92
#define _SC_TTY_NAME_MAX                        93
#define _SC_TYPED_MEMORY_OBJECTS                94
#define _SC_TZNAME_MAX                          95
#define _SC_V8_ILP32_OFF32                      96
#define _SC_V8_ILP32_OFFBIG                     97
#define _SC_V8_LP64_OFF64                       98
#define _SC_V8_LPBIG_OFFBIG                     99
#define _SC_V7_ILP32_OFF32                      100
#define _SC_V7_ILP32_OFFBIG                     101
#define _SC_V7_LP64_OFF64                       102
#define _SC_V7_LPBIG_OFFBIG                     103
#define _SC_VERSION                             104
#define _SC_XOPEN_CRYPT                         105
#define _SC_XOPEN_ENH_I18N                      106
#define _SC_XOPEN_REALTIME                      107
#define _SC_XOPEN_REALTIME_THREADS              108
#define _SC_XOPEN_SHM                           109
#define _SC_XOPEN_UNIX                          110
#define _SC_XOPEN_UUCP                          111
#define _SC_XOPEN_VERSION                       112

/* Integer sizes */
#define CHAR_MAX    SCHAR_MAX
#define CHAR_MIN    SCHAR_MIN
#define UCHAR_MIN   0
#define UCHAR_MAX   255

#define SCHAR_MAX   _#define _SCHAR_MAX__
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
