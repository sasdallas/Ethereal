/**
 * @file libpolyhedron/include/unistd.h
 * @brief unistd
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

#ifndef _UNISTD_H
#define _UNISTD_H

/**** INCLUDES ****/
#include <sys/syscall.h>
#include <sys/syscall_defs.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <fcntl.h>
#include <stddef.h>
#include <sys/types.h>
#include <errno.h>
#include <time.h>

/**** VARIABLES ****/

extern char **environ;

/**** FUNCTIONS ****/

void exit(int status);
int open(const char *pathname, int flags, ...);
ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
int close(int fd);
int stat(const char *pathname, struct stat *statbuf);
int fstat(int fd, struct stat *statbuf);
int lstat(const char *pathname, struct stat *statbuf);
int ioctl(int fd, unsigned long request, ...);
int brk(void *addr);
void *sbrk(intptr_t increment);
pid_t fork();
off_t lseek(int fd, off_t offset, int whence);
int usleep(useconds_t usec);
int execvpe(const char *file, const char *argv[], char *envp[]);
int execve(const char *pathname, const char *argv[], char *envp[]);
int execv(const char *path, const char *argv[]);
int execl(const char *pathname, const char *arg, ...);
int execle(const char *path, const char *arg, ...);
int execlp(const char *file, const char *arg, ...);
pid_t wait(int *wstatus);
pid_t waitpid(pid_t pid, int *wstatus, int options);
char *getcwd(char *buf, size_t size);
int chdir(const char *path);
int fchdir(int fd);
pid_t getpid();

/* STUBS */
int mkdir(const char *pathname, mode_t mode);
int remove(const char *pathname);
int rename(const char *oldpath, const char *newpath);
int system(const char *command);

#endif

_End_C_Header