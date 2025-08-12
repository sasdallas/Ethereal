/**
 * @file libpolyhedron/stdlib/ptsname.c
 * @brief ptsname
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <stdlib.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/libc_debug.h>

char *ptsname(int fd) {
    if (!isatty(fd)) return NULL;
    dprintf("libc: ptsname: stub\n");

    // TODO: properly do this?
    return "/device/tty0";
}