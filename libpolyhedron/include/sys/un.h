/**
 * @file libpolyhedron/include/sys/un.h
 * @brief UNIX sockets
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

#ifndef _SYS_UN_H
#define _SYS_UN_H

/**** INCLUDES ****/
#include <sys/socket.h>

/**** TYPES ****/

struct sockaddr_un {
    sa_family_t sun_family;
    char sun_path[108];
};

#endif