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
#include <sys/times.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <termios.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/epoll.h>
#include <ethereal/driver.h>
#include <ethereal/shared.h>
#include <unistd.h>
#include <dirent.h>
#include <signal.h>
#include <poll.h>

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

/**
 * @brief mmap context
 * 
 * This is used to bypass the 5 parameter limit
 */
typedef struct sys_mmap_context {
    void *addr;
    size_t len;
    int prot;
    int flags;
    int filedes;
    off_t off;
} sys_mmap_context_t;

typedef struct sys_setopt_context {
    int socket;
    int level;
    int option_name;
    const void *option_value;
    socklen_t option_len;
} sys_setopt_context_t;

typedef struct sys_pselect_context {
    int nfds;
    fd_set *readfds;
    fd_set *writefds;
    fd_set *errorfds;
    const struct timespec *timeout;
    const sigset_t *sigmask;
} sys_pselect_context_t;

/**** MACROS ****/

/* Pointer validation */
#define SYSCALL_VALIDATE_PTR(ptr) if (!mem_validate((void*)ptr, PTR_USER | PTR_STRICT)) syscall_pointerValidateFailed((void*)ptr);

/* Pointer validation (range) */
#define SYSCALL_VALIDATE_PTR_SIZE(ptr, size) for (uintptr_t p = (uintptr_t)(ptr); p < (uintptr_t)((ptr) + (size)); p += PAGE_SIZE) { SYSCALL_VALIDATE_PTR(p); }

/* Prototypes to avoid flooding header files */
struct msghdr;

/**** FUNCTIONS ****/

/**
 * @brief Handle a system call
 * @param syscall The system call to handle
 * @returns Nothing, but updates @c syscall->return_value
 */
void syscall_handle(syscall_t *syscall);

/**
 * @brief Pointer validation failed
 * @param ptr The pointer that failed to validate
 * @returns Only if resolved.
 */
void syscall_pointerValidateFailed(void *ptr);

/**
 * @brief Finish a system call after setting registers
 */
void syscall_finish();


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
long sys_poll(struct pollfd fds[], nfds_t nfds, int timeout);
long sys_mkdir(const char *pathname, mode_t mode);
long sys_pselect(sys_pselect_context_t *ctx);
ssize_t sys_readlink(const char *path, char *buf, size_t bufsiz);
long sys_access(const char *path, int amode);
long sys_chmod(const char *path, mode_t mode);
long sys_fcntl(int fd, int cmd, int extra);
long sys_unlink(const char *pathname);
long sys_ftruncate(int fd, off_t length);
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
clock_t sys_times(struct tms *buf);
long sys_mmap(sys_mmap_context_t *context);
long sys_mprotect(void *addr, size_t len, int prot);
long sys_munmap(void *addr, size_t len);
long sys_msync(void *addr, size_t len, int flags);
long sys_dup2(int oldfd, int newfd);
long sys_signal(int signum, void (*handler)(int));
long sys_sigaction(int signum, const struct sigaction *act, struct sigaction *oact);
long sys_sigpending(sigset_t *set);
long sys_sigprocmask(int how, const sigset_t *set, sigset_t *oset);
long sys_sigsuspend(const sigset_t *sigmask);
long sys_sigwait(const sigset_t *set, int *sig);
long sys_kill(pid_t pid, int sig);
long sys_socket(int domain, int type, int protocol);
ssize_t sys_sendmsg(int socket, struct msghdr *message, int flags);
ssize_t sys_recvmsg(int socket, struct msghdr *message, int flags);
long sys_getsockopt(int socket, int level, int option_name, void *option_value, socklen_t *option_len);
long sys_setsockopt(sys_setopt_context_t *context);
long sys_bind(int socket, const struct sockaddr *addr, socklen_t addrlen);
long sys_connect(int socket, const struct sockaddr *addr, socklen_t addrlen);
long sys_listen(int socket, int backlog);
long sys_accept(int socket, struct sockaddr *addr, socklen_t *addrlen);
long sys_getsockname(int socket, struct sockaddr *address, socklen_t *address_len);
long sys_getpeername(int socket, struct sockaddr *address, socklen_t *address_len);
long sys_mount(const char *src, const char *dst, const char *type, unsigned long flags, const void *data);
long sys_umount(const char *mountpoint);
long sys_pipe(int fildes[2]);
long sys_epoll_create(int size);
long sys_epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
long sys_epoll_pwait(int epfd, struct epoll_event *events, int maxevents, int timeout, const sigset_t *sigmask);
long sys_openpty(int *amaster, int *aslave, char *name, const struct termios *termp, const struct winsize *winp);
uid_t sys_getuid();
int sys_setuid(uid_t uid);
gid_t sys_getgid();
int sys_setgid(gid_t gid);
pid_t sys_getppid();
pid_t sys_getpgid(pid_t pid);
int sys_setpgid(pid_t pid, pid_t pgid);
pid_t sys_getsid();
pid_t sys_setsid();
uid_t sys_geteuid();
int sys_seteuid(uid_t uid);
gid_t sys_getegid();
int sys_setegid(gid_t gid);
long sys_gethostname(char *name, size_t size);
long sys_sethostname(const char *name, size_t size);
long sys_yield();
long sys_setitimer(int which, const struct itimerval *value, struct itimerval *ovalue);
long sys_ptrace(enum __ptrace_request op, pid_t pid, void *addr, void *data);
long sys_read_entries(int handle, void *buffer, size_t max_size);
long sys_futex_wait(int *pointer, int expected, const struct timespec *time);
long sys_futex_wake(int *pointer);
long sys_openat(int dirfd, const char *pathname, int flags, mode_t mode);

/* Ethereal system calls */
long sys_create_thread(uintptr_t stack, uintptr_t tls, void *entry, void *arg);
long sys_exit_thread(void *retval);
pid_t sys_gettid();
int sys_settls(uintptr_t tls);
long sys_join_thread(pid_t tid, void **retval);
long sys_kill_thread(pid_t tid, int sig);

long sys_ethereal_shared_new(size_t size, int flags);
key_t sys_ethereal_shared_key(int fd);
long sys_ethereal_shared_open(key_t key);

long sys_load_driver(char *filename, int priority, char **argv);
long sys_unload_driver(pid_t id);
long sys_get_driver(pid_t id, ethereal_driver_t *driver);

long sys_reboot(int operation);

#endif