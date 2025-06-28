/**
 * @file libpolyhedron/include/fcntl.h
 * @brief File control options
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <sys/cheader.h>

_Begin_C_Header

#ifndef _FCNTL_H
#define _FCNTL_H

/**** INCLUDES ****/
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>

/**** DEFINITIONS ****/

/* Open bitflags */
#define O_RDONLY        0x00000000
#define O_WRONLY        0x00000001
#define O_RDWR          0x00000002
#define O_CREAT         0x00000100
#define O_EXCL          0x00000200
#define O_NOCTTY        0x00000400
#define O_TRUNC         0x00001000
#define O_APPEND        0x00002000
#define O_NONBLOCK      0x00004000
#define O_DSYNC         0x00010000
#define O_DIRECT        0x00040000
#define O_LARGEFILE     0x00100000
#define O_DIRECTORY     0x00200000
#define O_NOFOLLOW      0x00400000
#define O_NOATIME       0x01000000
#define O_CLOEXEC       0x02000000
#define O_PATH          0x04000000


/* fcntl codes */
#define F_DUPFD             0
#define F_GETFD             1
#define F_SETFD             2
#define F_GETFL             3
#define F_SETFL             4
#define F_GETLK             5
#define F_SETLK             6
#define F_SETLKW            7
#define F_GETOWN            8
#define F_SETOWN            9

/* Record locking values */
#define F_RDLCK             1
#define F_UNLCK             2
#define F_WRLCK             3

/* File descriptor flags */
#define FD_CLOEXEC          0x1

/**** TYPES ****/

struct flock {
    short l_type;
    short l_whence;
    off_t l_start;
    off_t l_len;
    pid_t l_pid;
};

/**** FUNCTIONS ****/

int  creat(const char *, mode_t);
int  fcntl(int, int, ...);
int  open(const char *, int, ...);

#endif

_End_C_Header