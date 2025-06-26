/**
 * @file libpolyhedron/include/poll.h
 * @brief poll.h
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

#ifndef _POLL_H
#define _POLL_H

/**** DEFINITIONS ****/

#define POLLRDNORM              0x01    // Data on priority band 0 may be read
#define POLLRDBAND              0x02    // Data on priority bands greater than 0 may be read
#define POLLPRI                 0x04    // High priority data may be read
#define POLLWRNORM              0x08    // Data on priority band 0 may be written
#define POLLWRBAND              0x10    // Data on priority bands greater than 0 may be written
#define POLLERR                 0x20    // An error has occurred
#define POLLHUP                 0x40    // Device has been disconnected
#define POLLNVAL                0x80    // Invalid fd member

#define POLLIN                  (POLLRDNORM | POLLRDBAND)
#define POLLOUT                 (POLLWRNORM)

/**** TYPES ****/

struct pollfd {
    int fd;                 // The folowing descriptor being polled
    short events;           // The input event flags
    short revents;          // The output event flags
};

typedef unsigned int nfds_t;

/**** FUNCTIONS ****/

int poll(struct pollfd fds[], nfds_t nfds, int timeout);

#endif

_End_C_Header