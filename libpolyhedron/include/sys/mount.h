/**
 * @file libpolyhedron/include/sys/mount.h
 * @brief mount
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

#ifndef _SYS_MOUNT_H
#define _SYS_MOUNT_H

/* TODO: Other parts of this file */

/**** FUNCTIONS ****/

int mount(const char *source, const char *target, const char *filesystemtype, unsigned long mountflags, const void *data);

#endif