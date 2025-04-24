/**
 * @file hexahedron/include/kernel/task/syscall.h
 * @brief System call handler
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_TASK_SYSCALL_H
#define KERNEL_TASK_SYSCALL_H

/**** INCLUDES ****/
#include <stdint.h>
#include <sys/types.h>
#include <stddef.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/wait.h>

/**** DEFINITIONS ****/

#define SYSCALL_MAX_PARAMETERS      6       // !!!: AT THE CURRENT TIME, ONLY 5 ARE SUPPORTED

/**** TYPES ****/

/**
 * @brief System call structure
 */
typedef struct syscall {
    int syscall_number;
    long parameters[SYSCALL_MAX_PARAMETERS];
    long return_value;
} syscall_t;

/**
 * @brief System call function
 * 
 * @warning We're treading in unknown waters here - overloading functions
 */
typedef long (*syscall_func_t)(long, long, long, long, long);

/**** MACROS ****/

/* Pointer validation */
#define SYSCALL_VALIDATE_PTR(ptr) if (!mem_validate((void*)ptr, PTR_USER | PTR_STRICT)) syscall_pointerValidateFailed((void*)ptr);

/* Pointer validation (range) */
#define SYSCALL_VALIDATE_PTR_SIZE(ptr, size) for (uintptr_t i = (uintptr_t)ptr; i < (uintptr_t)ptr + (uintptr_t)size; i += PAGE_SIZE) { SYSCALL_VALIDATE_PTR(i); }


/**** FUNCTIONS ****/

/**
 * @brief Handle a system call
 * @param syscall The system call to handle
 * @returns Nothing, but updates @c syscall->return_value
 */
void syscall_handle(syscall_t *syscall);

/* System calls */
void sys_exit(int status);
int sys_open(const char *pathname, int flags, mode_t mode);
ssize_t sys_read(int fd, void *buffer, size_t count);
ssize_t sys_write(int fd, const void *buffer, size_t count);
int sys_close(int fd);
long sys_stat(const char *pathname, struct stat *statbuf);
long sys_fstat(int fd, struct stat *statbuf);
long sys_lstat(const char *pathname, struct stat *statbuf);
long sys_ioctl(int fd, unsigned long request, void *argp);
long sys_readdir(struct dirent *ent, int fd, unsigned long index);
void *sys_brk(void *addr);
pid_t sys_fork();
off_t sys_lseek(int fd, off_t offset, int whence);
long sys_gettimeofday(struct timeval *tv, void *tz);
long sys_settimeofday(struct timeval *tv, void *tz);
long sys_usleep(useconds_t usec);
long sys_execve(const char *pathname, const char *argv[], const char *envp[]);
long sys_wait(pid_t pid, int *wstatus, int options);
long sys_getcwd(char *buf, size_t size);
long sys_chdir(const char *path);
long sys_fchdir(int fd);
long sys_uname(struct utsname *buf);
pid_t sys_getpid();

#endif