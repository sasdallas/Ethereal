/**
 * @file libpolyhedron/termios/tcsetpgrp.c
 * @brief tcsetpgrp
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

#include <termios.h>
#include <sys/ioctl.h>
#include <unistd.h>

int tcsetpgrp(int fd, pid_t p) {
    return ioctl(fd, TIOCSPGRP, &p);
}