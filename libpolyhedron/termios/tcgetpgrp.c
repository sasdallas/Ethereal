/**
 * @file libpolyhedron/termios/tcgetpgrp.c
 * @brief tcgetpgrp
 * 
 * @warning Technically a unistd function
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <unistd.h>
#include <sys/ioctl.h>
#include <termios.h>

pid_t tcgetpgrp(int fd) {
    pid_t p = -1;
    long ret = ioctl(fd, TIOCGPGRP, &p);
    if (ret != 0) return ret;
    return p;
}