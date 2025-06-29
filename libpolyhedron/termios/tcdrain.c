/**
 * @file libpolyhedron/termios/tcdrain.c
 * @brief tcdrain
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

int tcdrain(int fd) {
    errno = ENOTSUP;
    return -1;
}