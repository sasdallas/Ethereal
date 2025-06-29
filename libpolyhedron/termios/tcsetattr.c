/**
 * @file libpolyhedron/termios/tcsetattr.c
 * @brief tcsetattr
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

int tcsetattr(int fd, int optional, const struct termios *tios) {
    switch (optional) {
        case TCSADRAIN:
            return ioctl(fd, TCSETSW, tios);
        case TCSAFLUSH:
            return ioctl(fd, TCSETSF, tios);
        case TCSANOW:
        default:
            return ioctl(fd, TCSETS, tios);
    }
}