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
// !!!: Convenience definitions
#include <sys/syscall.h>    
#include <errno.h>

#include <sys/types.h>
#include <stddef.h>
#include <stdint.h> // intptr_t in sbrk

/**** VARIABLES ****/

extern char **environ;

/**** DEFINITIONS ****/

/* POSIX/XOPEN version */
#define _POSIX_VERSION      200809L     // POSIX.1-2017
#define _POSIX2_VERSION     200809L     // POSIX.1-2017
#define _XOPEN_VERSION      700

/* POSIX features */

/**
 * This is the Ethereal POSIX feature set
 * A value of -1 means that the feature is unsupported.
 * A value of 0 means the feature might be supported.
 * A value greater than 0 means the feature is always supported
 */

#define _POSIX_ADVISORY_INFO                    -1              // posix_fadvice, posix_fallocate, posix_memalign, posix_madvice
#define _POSIX_ASYNCRONOUS_IO                   -1              // aio_ functions
#define _POSIX_BARRIERS                         _POSIX_VERSION
#define _POSIX_CHOWN_RESTRICTED                 _POSIX_VERSION 
#define _POSIX_CLOCK_SELECTION                  0               // pthread_attr_*clock, clock_nanosleep
#define _POSIX_CPUTIME                          -1              // CLOCK_CPUTIME
#define _POSIX_FSYNC                            -1
#define _POSIX_IPV6                             0               // IPv6
#define _POSIX_JOB_CONTROL                      _POSIX_VERSION  // tcdrain, tcsendbreak
#define _POSIX_MAPPED_FILES                     _POSIX_VERSION  // mmap files
#define _POSIX_MEMLOCK                          -1              // mlockall, munlockall
#define _POSIX_MEMLOCK_RANGE                    -1              // mlock, munlock
#define _POSIX_MEMORY_PROTECTION                _POSIX_VERSION  // mprotect
#define _POSIX_MESSAGE_PASSING                  -1              // mq functions
#define _POSIX_MONOTONIC_CLOCK                  -1              // Monotonic Clock
#define _POSIX_NO_TRUNC                         _POSIX_VERSION  // PATH_MAX check support
#define _POSIX_PRIORITIZED_IO                   -1              // aio_ functions
#define _POSIX_PRIORITY_SCHEDUING               -1              // Functions not provided yet
#define _POSIX_RAW_SOCKETS                      _POSIX_VERSION  // Raw sockets are supported
#define _POSIX_READER_WRITER_LOCKS              _POSIX_VERSION  // rwlock
#define _POSIX_REALTIME_SIGNALS                 _POSIX_VERSION
#define _POSIX_REGEXP                           0
#define _POSIX_SAVED_IDS                        _POSIX_VERSION  // Saved SUID and SGID
#define _POSIX_SEMAPHORES                       0               // sem_
#define _POSIX_SHARED_MEMORY_OBJECTS            -1              // shm_
#define _POSIX_SHELL                            _POSIX_VERSION
#define _POSIX_SPAWN                            -1              // posixspawn
#define _POSIX_SPIN_LOCKS                       -1
#define _POSIX_SPORADIC_SERVER                  -1              // sched_setparam, schedsetscheduler
#define _POSIX_SYNCRONIZED_IO                   _POSIX_VERSION  
#define _POSIX_THREAD_ATTR_STACKATTR            0               // pthread_attr_getstack*, pthread_attr_setstack
#define _POSIX_THREAD_ATTR_STACKSIZE            0               // pthread_attr_getstacksize, pthread_attr_setstacksize
#define _POSIX_THREAD_CPUTIME                   -1
#define _POSIX_THREAD_PRIO_INHERIT              _POSIX_VERSION
#define _POSIX_THREAD_PRIO_PROTECT              0
#define _POSIX_THREAD_PRIORITY_SCHEDULING       _POSIX_VERSION
#define _POSIX_THREAD_ROBUST_PRIO_INHERIT       0
#define _POSIX_THREAD_SAFE_FUNCTIONS            _POSIX_VERSION  // _r functions
#define _POSIX_THREAD_SPORADIC_SERVER           -1
#define _POSIX_THREADS                          _POSIX_VERSION
#define _POSIX_TIMEOUTS                         -1              // mq_timed*
#define _POSIX_TIMERS                           -1              // timer_*
#define _POSIX_TRACE                            -1              // posix_trace_*
#define _POSIX_TRACE_EVENT_FILTER               -1
#define _POSIX_TRACE_LOG                        -1
#define _POSIX_TYPED_MEMORY_OBJECTS             -1              // posix_mem_offset

/* access flags */
#define F_OK    0
#define R_OK    4
#define W_OK    2
#define X_OK    1

/* temporary */
#define _PC_PATH_MAX    256


/**** FUNCTIONS ****/

void __attribute__((noreturn)) _exit(int status);
void __attribute__((noreturn)) exit(int status);
int open(const char *pathname, int flags, ...);
ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
int close(int fd);
int ioctl(int fd, unsigned long request, ...);
int brk(void *addr);
void *sbrk(intptr_t increment);
pid_t fork();
off_t lseek(int fd, off_t offset, int whence);
int usleep(useconds_t usec);
unsigned int sleep(unsigned int seconds);
int execvpe(const char *file, const char *argv[], char *envp[]);
int execvp(const char *file, const char *argv[]);
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
int dup2(int oldfd, int newfd);
int dup(int fd);
int getopt(int argc, char * const argv[], const char * optstring);
int pipe(int fildes[2]);
int mkdir(const char *pathname, mode_t mode);
int remove(const char *pathname);
int rename(const char *oldpath, const char *newpath);
int system(const char *command);
int unlink(const char *pathname);
int access(const char *path, int amode);
int isatty(int fd);
long pathconf(const char *path, int name);
long fpathconf(int fd, int name);
pid_t getppid();
gid_t getegid();
uid_t geteuid();
pid_t getsid();
pid_t getpgid(pid_t pid);
gid_t getgid();
pid_t getppid();
uid_t getuid();
int setuid(uid_t uid);
int setgid(gid_t gid);
pid_t setpgrp();
pid_t setsid();
int seteuid(uid_t uid);
int setegid(gid_t gid);
int setpgid(pid_t pid, pid_t pgid);
char *ttyname(int fd);
int rmdir(const char *pathname);
char *getlogin(void);
pid_t tcgetpgrp(int);
int tcsetpgrp(int, pid_t);
int symlink(const char *path1, const char *path2);
int link(const char *oldpath, const char *newpath);
int gethostname(char *name, size_t size);
int sethostname(const char *name, size_t size);

#endif

_End_C_Header