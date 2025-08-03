/**
 * @file libpolyhedron/include/sys/mman.h
 * @brief mman
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <sys/cheader.h>

_Begin_C_Header;

#ifndef _SYS_MMAN_H
#define _SYS_MMAN_H

/**** INCLUDES ****/
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

/**** DEFINITIONS ****/

/* mmap protections */
#define PROT_READ       0x01
#define PROT_WRITE      0x02
#define PROT_EXEC       0x04
#define PROT_NONE       0x08

/* mmap flags */
#define MAP_FAILED      (void*)-1   // Map failure
#define MAP_SHARED      0x01        // Share changes
#define MAP_PRIVATE     0x02        // Changes are private
#define MAP_FIXED       0x04        // Interpret addr exactly
#define MAP_ANONYMOUS   0x08        // Anonymous memory mapping

/* msync flags */
#define MS_ASYNC        0x01
#define MS_SYNC         0x02
#define MS_INVALIDATE   0x04

/* madvise flags */
#define POSIX_MADV_DONTNEED		1
#define POSIX_MADV_NORMAL		2
#define POSIX_MADV_RANDOM		3
#define POSIX_MADV_SEQUENTIAL	4
#define POSIX_MADV_WILLNEED		5

#define MADV_DONTNEED	POSIX_MADV_DONTNEED
#define MADV_NORMAL		POSIX_MADV_NORMAL
#define MADV_RANDOM		POSIX_MADV_RANDOM
#define MADV_SEQUENTIAL	POSIX_MADV_SEQUENTIAL
#define MADV_WILLNEED	POSIX_MADV_WILLNEED


/**** FUNCTIONS ****/
void *mmap(void *addr, size_t len, int prot, int flags, int fildes, off_t off);
int munmap(void *addr, size_t len);
int mprotect(void *addr, size_t len, int prot);
int msync(void *addr, size_t len, int flags);
int madvise(void *addr, size_t len, int advice);

#endif

_End_C_Header;