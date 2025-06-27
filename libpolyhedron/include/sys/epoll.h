/**
 * @file libpolyhedron/include/sys/epoll.h
 * @brief epoll.h
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

#ifndef _SYS_EPOLL_H
#define _SYS_EPOLL_H

/**** INCLUDES ****/
#include <stdint.h>
#include <signal.h>

/**** DEFINITIONS ****/

#define EPOLL_CTL_ADD       0
#define EPOLL_CTL_MOD       1
#define EPOLL_CTL_DEL       2

#define EPOLLIN             0x01
#define EPOLLOUT            0x02
#define EPOLLPRI            0x04
#define EPOLLERR            0x08
#define EPOLLHUP            0x10
#define EPOLLET             0x20
#define EPOLLONESHOT        0x40

/**** TYPES ****/

union epoll_data {
    void     *ptr;
    int       fd;
    uint32_t  u32;
    uint64_t  u64;
};

typedef union epoll_data epoll_data_t;

struct epoll_event {
    uint32_t      events;
    epoll_data_t  data;
};

/**** FUNCTIONS ****/

int epoll_create(int size);
int epoll_create1(int flags);
int epoll_ctl(int epfd, int op, int fd, struct epoll_event* event);
int epoll_wait(int epfd, struct epoll_event* events, int maxevents, int timeout);
int epoll_pwait(int epfd, struct epoll_event* events, int maxevents, int timeout, const sigset_t* sigmask);

#endif

_End_C_Header