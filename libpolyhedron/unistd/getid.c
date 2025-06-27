/**
 * @file libpolyhedron/unistd/getid.c
 * @brief getid functions
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
#include <stdio.h>
#include <errno.h>

gid_t getegid() { return 0; }
uid_t geteuid() { return 0; }
gid_t getgid() { return 0; }
pid_t getppid() { return 0; }
uid_t getuid() { return 0; }