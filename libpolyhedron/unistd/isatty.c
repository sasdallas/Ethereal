/**
 * @file libpolyhedron/unistd/isatty.c
 * @brief isatty
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>

int isatty(int fd) {
    int tty = 0;
    int r = ioctl(fd, IOCTLTTYIS, &tty); 
    if (r < 0) {
        if (errno != EBADF) errno = ENOTTY;
        return 0;
    }

    return tty;
}