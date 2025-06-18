/**
 * @file libpolyhedron/include/sys/stat.h
 * @brief stat
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

#ifndef _SYS_STAT_H
#define _SYS_STAT_H


/**** INCLUDES ****/
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>

/**** DEFINITIONS ****/

#define S_IFBLK         0x01        // Block special
#define S_IFCHR         0x02        // Character special
#define S_IFIFO         0x04        // FIFO special
#define S_IFREG         0x08        // Regular
#define S_IFDIR         0x10        // Directory
#define S_IFLNK         0x20        // Symbolic link
#define S_IFSOCK        0x40        // Socket
#define S_IFMT          0x7F        // Format of file

#define S_IRUSR         0x001   // Read permission, owner
#define S_IWUSR         0x002   // Write permission, owner
#define S_IXUSR         0x004   // Execute/search permission, owner
#define S_IRGRP         0x008   // Read permission, group
#define S_IWGRP         0x010   // Write permission, group
#define S_IXGRP         0x020   // Execute/search permission, group
#define S_IROTH         0x040   // Read permission, others
#define S_IWOTH         0x080   // Write permission, others
#define S_IXOTH         0x100   // Execute/search permission, others
#define S_ISUID         0x200   // Set user ID on execution
#define S_ISGID         0x400   // Set grou ID on execution
#define S_ISVTX         0x800   // On directories, restricted deletion flag

#define S_IRWXU         (S_IRUSR | S_IWUSR | S_IXUSR)
#define S_IRWXG         (S_IRGRP | S_IWGRP | S_IXGRP)
#define S_IRWXO         (S_IROTH | S_IWOTH | S_IXOTH)

/* Kernel default block size */
#define STAT_DEFAULT_BLOCK_SIZE     512

/**** MACROS ****/

#define S_ISBLK(m) ((m & S_IFMT) == S_IFBLK)
#define S_ISCHR(m) ((m & S_IFMT) == S_IFCHR)
#define S_ISDIR(m) ((m & S_IFMT) == S_IFDIR)
#define S_ISFIFO(m) ((m & S_IFMT) == S_IFIFO)
#define S_ISREG(m) ((m & S_IFMT) == S_IFREG)
#define S_ISLNK(m) ((m & S_IFMT) == S_IFLNK)
#define S_ISSOCK(m) ((m & S_IFMT) == S_IFSOCK)

/**** TYPES ****/

struct stat {
    dev_t st_dev;
    ino_t st_ino;
    mode_t st_mode;
    nlink_t st_nlink;
    uid_t st_uid;
    gid_t st_gid;
    dev_t st_rdev;
    off_t st_size;
    time_t st_atime;
    time_t st_mtime;
    time_t st_ctime;
    blksize_t st_blksize;
    blkcnt_t st_blocks;
};

/**** FUNCTIONS ****/

int stat(const char *pathname, struct stat *statbuf);
int fstat(int fd, struct stat *statbuf);
int lstat(const char *pathname, struct stat *statbuf);

#endif