/**
 * @file libpolyhedron/include/dirent.h
 * @brief dirent
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

#ifndef _DIRENT_H
#define _DIRENT_H

/**** INCLUDES ****/
#include <stdint.h>
#include <bits/dirent.h>
#include <sys/types.h>

/**** DEFINITIONS ****/
#define DT_BLK      0
#define DT_CHR      1
#define DT_DIR      2
#define DT_FIFO     3
#define DT_LNK      4
#define DT_REG      5
#define DT_SOCK     6
#define DT_UNKNOWN  7

/**** TYPES ****/

typedef struct DIR {
    int fd;                 // File descriptor of this DIR
    int current_index;      // Current index in the DIR
} DIR;

/**** FUNCTIONS ****/

int closedir(DIR *dirp);
DIR *fdopendir(int fd);
DIR *opendir(const char *path);
struct dirent *readdir(DIR *dirp);
int readdir_r(DIR *dirp, struct dirent *entry, struct dirent **result);
void rewinddir(DIR *dirp);
void seekdir(DIR *dirp, long int loc);
long int telldir(DIR *dirp);

#endif

_End_C_Header