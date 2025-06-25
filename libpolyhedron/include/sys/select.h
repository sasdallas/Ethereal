
/**
 * @file libpolyhedron/include/sys/select.h
 * @brief select.h
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <sys/cheader.h>

_Begin_C_Header;

#ifndef _SYS_SELECT_H
#define _SYS_SELECT_H

/**** INCLUDES ****/
#include <stdint.h>
#include <sys/time.h>
#include <signal.h>
#include <time.h>

/**** DEFINITIONS ****/
#define FD_SETSIZE 1024

/**** TYPES ****/
typedef unsigned long fd_mask;

typedef struct _fd_set {
    fd_mask fds_bits[FD_SETSIZE / (sizeof(fd_mask) * 8)];
} fd_set;

/**** MACROS ****/
#define __FD_MASK_SIZE      (sizeof(fd_mask) * 8)

#define FD_CLR(fd, setp) ((setp)->fds_bits[(fd) / __FD_MASK_SIZE] &= ~((fd_mask)1 << ((fd) % __FD_MASK_SIZE)))
#define FD_ISSET(fd, setp) ((setp)->fds_bits[(fd) / __FD_MASK_SIZE] & ((fd_mask)1 << ((fd) % __FD_MASK_SIZE)))
#define FD_SET(fd, setp) ((setp)->fds_bits[(fd) / __FD_MASK_SIZE] |= ((fd_mask)1 << ((fd) % __FD_MASK_SIZE)))
#define FD_ZERO(setp) { for (unsigned i = 0; i < FD_SETSIZE / __FD_MASK_SIZE; i++) (setp)->fds_bits[i] = (fd_mask)0; }

/**** FUNCTIONS ****/

int pselect(int nfds, fd_set* readfds, fd_set* writefds, fd_set* errorfds, const struct timespec* timeout, const sigset_t* sigmask);
int select(int nfds, fd_set* readfds, fd_set* writefds, fd_set* errorfds, struct timeval* timeout);

#endif

_End_C_Header;