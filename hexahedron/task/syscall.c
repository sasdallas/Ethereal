/**
 * @file hexahedron/task/syscall.c
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

#include <kernel/task/syscall.h>
#include <kernel/task/process.h>
#include <kernel/loader/binfmt.h>
#include <kernel/drivers/net/socket.h>
#include <kernel/fs/vfs_new.h>
#include <kernel/fs/pipe.h>
#include <kernel/fs/tty.h>
#include <kernel/misc/args.h>
#include <kernel/mm/alloc.h>
#include <kernel/debug.h>
#include <kernel/panic.h>
#include <kernel/gfx/gfx.h>
#include <kernel/gfx/term.h>
#include <kernel/config.h>

#include <sys/syscall_nums.h>
#include <sys/types.h>
#include <sys/ptrace.h>
#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>

/* System call table */
static syscall_func_t syscall_table[] = {
    [SYS_EXIT]              = (syscall_func_t)(uintptr_t)sys_exit,
    [SYS_OPEN]              = (syscall_func_t)(uintptr_t)sys_open,
    [SYS_READ]              = (syscall_func_t)(uintptr_t)sys_read,
    [SYS_WRITE]             = (syscall_func_t)(uintptr_t)sys_write,
    [SYS_CLOSE]             = (syscall_func_t)(uintptr_t)sys_close,
    [SYS_STAT]              = (syscall_func_t)(uintptr_t)sys_stat,
    [SYS_FSTAT]             = (syscall_func_t)(uintptr_t)sys_fstat,
    [SYS_LSTAT]             = (syscall_func_t)(uintptr_t)sys_lstat,
    [SYS_IOCTL]             = (syscall_func_t)(uintptr_t)sys_ioctl,
    [SYS_READDIR]           = (syscall_func_t)(uintptr_t)NULL, // dead system call
    [SYS_POLL]              = (syscall_func_t)(uintptr_t)sys_poll,
    [SYS_MKDIR]             = (syscall_func_t)(uintptr_t)sys_mkdir,
    [SYS_PSELECT]           = (syscall_func_t)(uintptr_t)sys_pselect,
    [SYS_READLINK]          = (syscall_func_t)(uintptr_t)sys_readlink,
    [SYS_ACCESS]            = (syscall_func_t)(uintptr_t)sys_access,
    [SYS_CHMOD]             = (syscall_func_t)(uintptr_t)sys_chmod,
    [SYS_FCNTL]             = (syscall_func_t)(uintptr_t)sys_fcntl,
    [SYS_UNLINKAT]          = (syscall_func_t)(uintptr_t)sys_unlinkat,
    [SYS_FTRUNCATE]         = (syscall_func_t)(uintptr_t)sys_ftruncate,
    [SYS_BRK]               = (syscall_func_t)(uintptr_t)sys_brk,
    [SYS_FORK]              = (syscall_func_t)(uintptr_t)sys_fork,
    [SYS_LSEEK]             = (syscall_func_t)(uintptr_t)sys_lseek,
    [SYS_GETTIMEOFDAY]      = (syscall_func_t)(uintptr_t)sys_gettimeofday,
    [SYS_SETTIMEOFDAY]      = (syscall_func_t)(uintptr_t)sys_settimeofday,
    [SYS_USLEEP]            = (syscall_func_t)(uintptr_t)sys_usleep,
    [SYS_EXECVE]            = (syscall_func_t)(uintptr_t)sys_execve,
    [SYS_WAIT]              = (syscall_func_t)(uintptr_t)sys_wait,
    [SYS_GETCWD]            = (syscall_func_t)(uintptr_t)sys_getcwd,
    [SYS_CHDIR]             = (syscall_func_t)(uintptr_t)sys_chdir,
    [SYS_FCHDIR]            = (syscall_func_t)(uintptr_t)sys_fchdir,
    [SYS_UNAME]             = (syscall_func_t)(uintptr_t)sys_uname,
    [SYS_GETPID]            = (syscall_func_t)(uintptr_t)sys_getpid,
    [SYS_TIMES]             = (syscall_func_t)(uintptr_t)sys_times,
    [SYS_MMAP]              = (syscall_func_t)(uintptr_t)sys_mmap,
    [SYS_MUNMAP]            = (syscall_func_t)(uintptr_t)sys_munmap,
    [SYS_MSYNC]             = (syscall_func_t)(uintptr_t)sys_msync,
    [SYS_MPROTECT]          = (syscall_func_t)(uintptr_t)sys_mprotect,
    [SYS_DUP2]              = (syscall_func_t)(uintptr_t)sys_dup2,
    [SYS_SIGNAL]            = (syscall_func_t)(uintptr_t)sys_signal,
    [SYS_SIGACTION]         = (syscall_func_t)(uintptr_t)sys_sigaction,
    [SYS_SIGPENDING]        = (syscall_func_t)(uintptr_t)sys_sigpending,
    [SYS_SIGPROCMASK]       = (syscall_func_t)(uintptr_t)sys_sigprocmask,
    [SYS_SIGSUSPEND]        = (syscall_func_t)(uintptr_t)sys_sigsuspend,
    [SYS_SIGWAIT]           = (syscall_func_t)(uintptr_t)sys_sigwait,
    [SYS_KILL]              = (syscall_func_t)(uintptr_t)sys_kill,
    [SYS_SOCKET]            = (syscall_func_t)(uintptr_t)sys_socket,
    [SYS_SENDMSG]           = (syscall_func_t)(uintptr_t)sys_sendmsg,
    [SYS_RECVMSG]           = (syscall_func_t)(uintptr_t)sys_recvmsg,
    [SYS_GETSOCKOPT]        = (syscall_func_t)(uintptr_t)sys_getsockopt,
    [SYS_SETSOCKOPT]        = (syscall_func_t)(uintptr_t)sys_setsockopt,
    [SYS_BIND]              = (syscall_func_t)(uintptr_t)sys_bind,
    [SYS_CONNECT]           = (syscall_func_t)(uintptr_t)sys_connect,
    [SYS_LISTEN]            = (syscall_func_t)(uintptr_t)sys_listen,
    [SYS_ACCEPT]            = (syscall_func_t)(uintptr_t)sys_accept,
    [SYS_GETSOCKNAME]       = (syscall_func_t)(uintptr_t)sys_getsockname,
    [SYS_GETPEERNAME]       = (syscall_func_t)(uintptr_t)sys_getpeername,
    [SYS_SOCKETPAIR]        = (syscall_func_t)(uintptr_t)sys_socketpair,
    [SYS_MOUNT]             = (syscall_func_t)(uintptr_t)sys_mount,
    [SYS_UMOUNT]            = (syscall_func_t)(uintptr_t)sys_umount,
    [SYS_PIPE]              = (syscall_func_t)(uintptr_t)sys_pipe,
    [SYS_SHARED_NEW]        = (syscall_func_t)(uintptr_t)sys_ethereal_shared_new,
    [SYS_SHARED_KEY]        = (syscall_func_t)(uintptr_t)sys_ethereal_shared_key,
    [SYS_SHARED_OPEN]       = (syscall_func_t)(uintptr_t)sys_ethereal_shared_open,
    [SYS_CREATE_THREAD]     = (syscall_func_t)(uintptr_t)sys_create_thread,
    [SYS_GETTID]            = (syscall_func_t)(uintptr_t)sys_gettid,
    [SYS_SETTLS]            = (syscall_func_t)(uintptr_t)sys_settls,
    [SYS_EXIT_THREAD]       = (syscall_func_t)(uintptr_t)sys_exit_thread,
    [SYS_JOIN_THREAD]       = (syscall_func_t)(uintptr_t)sys_join_thread,
    [SYS_KILL_THREAD]       = (syscall_func_t)(uintptr_t)sys_kill_thread,
    [SYS_EPOLL_CREATE]      = (syscall_func_t)(uintptr_t)sys_epoll_create,
    [SYS_EPOLL_CTL]         = (syscall_func_t)(uintptr_t)sys_epoll_ctl,
    [SYS_EPOLL_PWAIT]       = (syscall_func_t)(uintptr_t)sys_epoll_pwait,
    [SYS_OPENPTY]           = (syscall_func_t)(uintptr_t)sys_openpty,
    [SYS_GETUID]            = (syscall_func_t)(uintptr_t)sys_getuid,
    [SYS_SETUID]            = (syscall_func_t)(uintptr_t)sys_setuid,
    [SYS_GETGID]            = (syscall_func_t)(uintptr_t)sys_getgid,
    [SYS_SETGID]            = (syscall_func_t)(uintptr_t)sys_setgid,
    [SYS_GETPPID]           = (syscall_func_t)(uintptr_t)sys_getppid,
    [SYS_GETPGID]           = (syscall_func_t)(uintptr_t)sys_getpgid,
    [SYS_SETPGID]           = (syscall_func_t)(uintptr_t)sys_setpgid,
    [SYS_GETSID]            = (syscall_func_t)(uintptr_t)sys_getsid,
    [SYS_SETSID]            = (syscall_func_t)(uintptr_t)sys_setsid,
    [SYS_GETEUID]           = (syscall_func_t)(uintptr_t)sys_geteuid,
    [SYS_SETEUID]           = (syscall_func_t)(uintptr_t)sys_seteuid,
    [SYS_GETEGID]           = (syscall_func_t)(uintptr_t)sys_getegid,
    [SYS_SETEGID]           = (syscall_func_t)(uintptr_t)sys_setegid,
    [SYS_GETHOSTNAME]       = (syscall_func_t)(uintptr_t)sys_gethostname,
    [SYS_SETHOSTNAME]       = (syscall_func_t)(uintptr_t)sys_sethostname,
    [SYS_YIELD]             = (syscall_func_t)(uintptr_t)sys_yield,
    [SYS_LOAD_DRIVER]       = (syscall_func_t)(uintptr_t)sys_load_driver,
    [SYS_UNLOAD_DRIVER]     = (syscall_func_t)(uintptr_t)sys_unload_driver,
    [SYS_GET_DRIVER]        = (syscall_func_t)(uintptr_t)sys_get_driver,
    [SYS_SETITIMER]         = (syscall_func_t)(uintptr_t)sys_setitimer,
    [SYS_PTRACE]            = (syscall_func_t)(uintptr_t)sys_ptrace,
    [SYS_REBOOT]            = (syscall_func_t)(uintptr_t)sys_reboot,
    [SYS_READ_ENTRIES]      = (syscall_func_t)(uintptr_t)sys_read_entries,
    [SYS_FUTEX_WAIT]        = (syscall_func_t)(uintptr_t)sys_futex_wait,
    [SYS_FUTEX_WAKE]        = (syscall_func_t)(uintptr_t)sys_futex_wake,
    [SYS_OPENAT]            = (syscall_func_t)(uintptr_t)sys_openat,
    [SYS_RENAMEAT]          = (syscall_func_t)(uintptr_t)sys_renameat,
    [SYS_LINKAT]            = (syscall_func_t)(uintptr_t)0xdeadbeef,
    [SYS_SYMLINKAT]         = (syscall_func_t)(uintptr_t)sys_symlinkat,
    [SYS_FCHMODAT]          = (syscall_func_t)(uintptr_t)0xdeadbeef,
    [SYS_MKNODAT]           = (syscall_func_t)(uintptr_t)0xdeadbeef,
    [SYS_FLOCK]             = (syscall_func_t)(uintptr_t)0xdeadbeef,
    [SYS_UMASK]             = (syscall_func_t)(uintptr_t)0xdeadbeef,
    [SYS_CLOCK_GETTIME]     = (syscall_func_t)(uintptr_t)sys_clock_gettime,
    [SYS_PREAD]             = (syscall_func_t)(uintptr_t)sys_pread,
    [SYS_PWRITE]            = (syscall_func_t)(uintptr_t)sys_pwrite,
    [SYS_GETRLIMIT]         = (syscall_func_t)(uintptr_t)0xdeadbeef,
    [SYS_SETRLIMIT]         = (syscall_func_t)(uintptr_t)0xdeadbeef,
    [SYS_PAUSE]             = (syscall_func_t)(uintptr_t)sys_pause
}; 


/* Unimplemented system call */
#define SYSCALL_UNIMPLEMENTED(syscall) ({ LOG(ERR, "[UNIMPLEMENTED] The system call \"%s\" is unimplemented\n", syscall); return 0; })

/* Log method */
#define LOG(status, ...) dprintf_module(status, "TASK:SYSCALL", __VA_ARGS__)

/**
 * @brief Pointer validation failed
 * @param ptr The pointer that failed to validate
 * @returns Only if resolved.
 */
void syscall_pointerValidateFailed(void *ptr) {
    vmm_fault_information_t i = {
        .address = (uintptr_t)ptr,
        .exception_type = VMM_FAULT_NONPRESENT | VMM_FAULT_READ,
        .from = VMM_FAULT_FROM_USER
    };

    int r = vmm_fault(&i);
    if (r == VMM_FAULT_RESOLVED) return;

    kernel_panic_prepare(KERNEL_BAD_ARGUMENT_ERROR);

    printf("*** Process \"%s\" tried to access an invalid pointer (%p)\n", current_cpu->current_process->name, ptr);
    dprintf(NOHEADER, COLOR_CODE_RED_BOLD "*** Process \"%s\" tried to access an invalid pointer (%p)\n\n" COLOR_CODE_RESET, current_cpu->current_process->name, ptr);

    kernel_panic_finalize();
}


/**
 * @brief Finish a system call after setting registers
 */
void syscall_finish() {
    ptrace_event(PROCESS_TRACE_SYSCALL); // Can only ptrace after we've configured exit registers
    timemonitor_updateSyscallExit();
}

/**
 * @brief Handle a system call
 * @param syscall The system call to handle
 * @returns Nothing, but updates @c syscall->return_value
 */
void syscall_handle(syscall_t *syscall) {
    // Enter
    timemonitor_updateSyscallEntry();
    ptrace_event(PROCESS_TRACE_SYSCALL);

    if (syscall->syscall_number == 999) {
        syscall->return_value = -EINVAL;
        return;
    }

    // Is the system call within bounds?
    if (syscall->syscall_number < 0 || syscall->syscall_number >= (int)(sizeof(syscall_table) / sizeof(*syscall_table))) {
        LOG(ERR, "Invalid system call %d received\n", syscall->syscall_number);
        syscall->return_value = -EINVAL;
        return;
    }

    // Call!
    syscall->return_value = (syscall_table[syscall->syscall_number])(
                                syscall->parameters[0], syscall->parameters[1], syscall->parameters[2],
                                syscall->parameters[3], syscall->parameters[4]);

                            
    if ((long)syscall->return_value < 0 && syscall->return_value != -EWOULDBLOCK) {
        LOG(WARN, "system call %d failed with error %d\n", syscall->syscall_number, syscall->return_value);
    }

    return;
}

/**
 * @brief Change data segment size
 */
void *sys_brk(void *addr) {
    assert(0); // sbrk sucks anyways
    return addr;
}

long sys_usleep(useconds_t usec) {
    sleep_time((usec / 1000000), (usec % 1000000));
    if (sleep_enter() == WAKEUP_SIGNAL) return -EINTR;

    return 0;
}

long sys_wait(pid_t pid, int *wstatus, int options) {
    if (wstatus) {
        SYSCALL_VALIDATE_PTR(wstatus);
    }

    return process_waitpid(pid, wstatus, options);
}

long sys_chmod(const char *path, mode_t mode) {
    SYSCALL_UNIMPLEMENTED("sys_chmod");
}


/* MMAP */

long sys_msync(void *addr, size_t len, int flush) {
    LOG(WARN, "sys_msync %p %d %d\n");
    return 0;
}

/* DUP */

long sys_dup2(int oldfd, int newfd) {
    if (!FD_VALIDATE(oldfd)) {
        return -EBADF;
    }

    int fd_out;
    int err = fd_duplicate(oldfd, newfd, &fd_out, true);
    if (err != 0) return err;
    return fd_out;
}

/* SIGNALS */

long sys_signal(int signum, void (*handler)(int)) {
    // Validate range
    if (signum < 0 || signum >= NSIG) return -EINVAL;
    if (signum == SIGKILL || signum == SIGSTOP) return -EINVAL; // Trying to set signals that cannot be handled

    // Is the handler special?
    void *old_handler = THREAD_SIGNAL(current_cpu->current_thread, signum).handler;
    if (handler == SIG_IGN) {
        THREAD_SIGNAL(current_cpu->current_thread, signum).handler = SIGNAL_ACTION_IGNORE;
    } else if (handler == SIG_DFL) {
        THREAD_SIGNAL(current_cpu->current_thread, signum).handler = SIGNAL_ACTION_DEFAULT;
    } else {
        // Yes, the kernel will "fail" to catch this if handler is an invalid argument
        // ..but this executes in usermode so it's really us who are laughing
        THREAD_SIGNAL(current_cpu->current_thread, signum).handler = handler;
    }

    THREAD_SIGNAL(current_cpu->current_thread, signum).flags = SA_RESTART;
    return (long)old_handler;
}

long sys_kill(pid_t pid, int sig) {
    // Check signal
    if (sig < 0 || sig >= NSIG) return -EINVAL;

    if (pid > 0 || pid < -1) {
        if (pid < -1) pid *= -1;
        process_t *proc = process_getFromPID(pid);
        if (!proc) return -ESRCH;

        return signal_send(proc, sig);
    } else if (!pid) {
        LOG(ERR, "Unimplemented: Send to every process group\n");
        return -ENOTSUP;
    } else if (pid == -1) {
        // TODO
        LOG(ERR, "Unimplemented: Send to every process possible\n");
        return -ENOTSUP;
    }

    // Unreachable
    return -EINVAL;
}

long sys_sigaction(int signum, const struct sigaction *act, struct sigaction *oact) {
    if (act) SYSCALL_VALIDATE_PTR(act);
    if (oact) SYSCALL_VALIDATE_PTR(oact);

    if (signum < 0 || signum >= NSIG) return -EINVAL;
    if (signum == SIGKILL || signum == SIGSTOP) return -EINVAL;

    // First, assign the old action
    if (oact) {
        oact->sa_handler = THREAD_SIGNAL(current_cpu->current_thread, signum).handler;
        oact->sa_mask = THREAD_SIGNAL(current_cpu->current_thread, signum).mask;
        oact->sa_flags = THREAD_SIGNAL(current_cpu->current_thread, signum).flags;

        if (oact->sa_handler == SIGNAL_ACTION_IGNORE) oact->sa_handler = SIG_IGN;
        if ((uintptr_t)oact->sa_handler <= (uintptr_t)SIGNAL_ACTION_CONTINUE) oact->sa_handler = SIG_DFL;
    }

    // Now, assign the new one
    if (act) {
        if (act->sa_handler == SIG_IGN) {
            THREAD_SIGNAL(current_cpu->current_thread, signum).handler = SIGNAL_ACTION_IGNORE;
        } else if (act->sa_handler == SIG_DFL) {
            THREAD_SIGNAL(current_cpu->current_thread, signum).handler = SIGNAL_ACTION_DEFAULT;
        } else {
            THREAD_SIGNAL(current_cpu->current_thread, signum).handler = act->sa_handler;
        }

        THREAD_SIGNAL(current_cpu->current_thread, signum).mask = act->sa_mask;
        THREAD_SIGNAL(current_cpu->current_thread, signum).flags = act->sa_flags;
    }

    return 0;
}

long sys_sigpending(sigset_t *set) {
    SYSCALL_VALIDATE_PTR(set);
    *set = current_cpu->current_thread->pending_signals;
    return 0;
}

long sys_sigprocmask(int how, const sigset_t *set, sigset_t *oset) {
    if (oset) {
        SYSCALL_VALIDATE_PTR(oset);
        *oset = current_cpu->current_thread->blocked_signals; // TODO: thread?
    }

    if (set) {
        SYSCALL_VALIDATE_PTR(set);

        // How do they want us to do this?
        switch (how) {
            case SIG_BLOCK:
                // Block a signal
                current_cpu->current_thread->blocked_signals |= *set;
                break;

            case SIG_UNBLOCK:
                // Unblock a signal 
                current_cpu->current_thread->blocked_signals &= ~(*set);
                break;

            case SIG_SETMASK:
                // Set mask
                current_cpu->current_thread->blocked_signals = *set;
                break;
            
            default:
                return -EINVAL;
        }
    }

    return 0;
}

long sys_sigsuspend(const sigset_t *sigmask) {
    LOG(ERR, "sigsuspend is unimplemented\n");
    return -ENOSYS;
}

long sys_sigwait(const sigset_t *set, int *sig) {
    LOG(ERR, "sigwait is unimplemented\n");
    return -ENOSYS;
}

/* SOCKETS */

long sys_socket(int domain, int type, int protocol) {
    return socket_create(current_cpu->current_process, domain, type, protocol);
}

long sys_sendmsg(int socket, struct msghdr *message, int flags) {
    if (flags) LOG(WARN, "sys_sendmsg: flags are 0x%x\n", flags);
    return socket_sendmsg(socket, message, flags);
}

long sys_recvmsg(int socket, struct msghdr *message, int flags) {
    if (flags) LOG(WARN, "sys_recvmsg: flags are 0x%x\n", flags);
    return socket_recvmsg(socket, message, flags);
}

long sys_getsockopt(int socket, int level, int option_name, void *option_value, socklen_t *option_len) {
    return socket_getsockopt(socket, level, option_name, option_value, option_len);
}

long sys_setsockopt(sys_setopt_context_t *context) {
    // !!!: I don't know why context is required..
    SYSCALL_VALIDATE_PTR(context);
    return socket_setsockopt(context->socket, context->level, context->option_name, context->option_value, context->option_len);
}

long sys_bind(int socket, const struct sockaddr *addr, socklen_t addrlen) {
    return socket_bind(socket, addr, addrlen);
}

long sys_connect(int socket, const struct sockaddr *addr, socklen_t addrlen) {
    return socket_connect(socket, addr, addrlen);
}

long sys_listen(int socket, int backlog) {
    return socket_listen(socket, backlog);
}

long sys_accept(int socket, struct sockaddr *addr, socklen_t *addrlen) {
    return socket_accept(socket, addr, addrlen);
}

long sys_getsockname(int socket, struct sockaddr *addr, socklen_t *addrlen) {
    return socket_getsockname(socket, addr, addrlen);
}

long sys_getpeername(int socket, struct sockaddr *addr, socklen_t *addrlen) {
    return socket_getpeername(socket, addr, addrlen);
}

/* MOUNTS */

long sys_mount(const char *src, const char *dst, const char *type, unsigned long flags, const void *data) {
    SYSCALL_VALIDATE_PTR(src);
    SYSCALL_VALIDATE_PTR(dst);
    if (type) SYSCALL_VALIDATE_PTR(type);
    if (data) SYSCALL_VALIDATE_PTR(data);
    
    if (!type) {
        // Type can be NULL right? (Didn't care to read docs)
        LOG(ERR, "Lack of type is not supported\n");
        return -ENOTSUP;
    }

    // The current process must be root to mount
    if (current_cpu->current_process->uid != 0) {
        return -EPERM;
    }

    if (strlen(src) > PATH_MAX) return -ENAMETOOLONG;
    if (strlen(dst) > PATH_MAX) return -ENAMETOOLONG; 

    // get filesystem
    vfs2_filesystem_t *fs = vfs_getFilesystem((char*)type);
    if (!fs) { return -ENODEV; }

    // Canonicalize paths
    char *src_canonicalized = kmalloc(strlen(src) + strlen(current_cpu->current_process->wd_path) + 1);
    char *dst_canonicalized = kmalloc(strlen(src) + strlen(current_cpu->current_process->wd_path) + 1);  

    if (vfs_canonicalize(current_cpu->current_process->wd_path, (char*)src, src_canonicalized)) { kfree(src_canonicalized); kfree(dst_canonicalized); return -EINVAL; }
    if (vfs_canonicalize(current_cpu->current_process->wd_path, (char*)dst, dst_canonicalized)) { kfree(src_canonicalized); kfree(dst_canonicalized); return -EINVAL; }

    // Try to mount filesystem type
    int success = vfs2_mount(fs, src_canonicalized, (char*)dst_canonicalized, 0, NULL);
    kfree(src_canonicalized);
    kfree(dst_canonicalized);

    return success;
}

long sys_umount(const char *mountpoint) {
    LOG(WARN, "sys_umount unimplemented\n");
    return -ENOTSUP;
}

/**** PIPES ****/

long sys_pipe(int fildes[2]) {
    SYSCALL_VALIDATE_PTR(fildes);
    return pipe_create(fildes);
}

/**** EPOLL ****/

long sys_epoll_create(int size) {
    return -ENOTSUP;
}

long sys_epoll_ctl(int epfd, int op, int fd, struct epoll_event *event) {
    return -ENOTSUP;
}

long sys_epoll_pwait(int epfd, struct epoll_event *events, int maxevents, int timeout, const sigset_t *sigmask) {
    return -ENOTSUP;
}

/**** SCHED ****/

long sys_yield() {
    process_yield(1);
    return 0; 
}

/**** TIMERS ****/

long sys_setitimer(int which, const struct itimerval *value, struct itimerval *ovalue) {
    if (which > ITIMER_PROF) return -EINVAL;

    // Check values
    if (value) SYSCALL_VALIDATE_PTR_SIZE(value, sizeof(struct itimerval));
    if (ovalue) SYSCALL_VALIDATE_PTR_SIZE(ovalue, sizeof(struct itimerval));

    if (!value && !ovalue) {
        return 0;
    }

    // Handle cases
    if (value) {
        if (ovalue) {
            // !!!: BUG HERE - it_value is not updated as the timer system sleeps. We can probably just do it here.
            ovalue->it_interval.tv_sec = current_cpu->current_process->itimers[which].reset_value.tv_sec;
            ovalue->it_interval.tv_usec = current_cpu->current_process->itimers[which].reset_value.tv_usec;
            ovalue->it_value.tv_sec = current_cpu->current_process->itimers[which].value.tv_sec;
            ovalue->it_value.tv_usec = current_cpu->current_process->itimers[which].value.tv_usec;
        }

        int r = timer_set(current_cpu->current_process, which, (struct itimerval*)value);
        if (r != 0) return r;
    } else {
        // They just want to get the timer in ovalue
            // !!!: BUG HERE - it_value is not updated as the timer system sleeps. We can probably just do it here.
        ovalue->it_interval.tv_sec = current_cpu->current_process->itimers[which].reset_value.tv_sec;
        ovalue->it_interval.tv_usec = current_cpu->current_process->itimers[which].reset_value.tv_usec;
        ovalue->it_value.tv_sec = current_cpu->current_process->itimers[which].value.tv_sec;
        ovalue->it_value.tv_usec = current_cpu->current_process->itimers[which].value.tv_usec;
    }


    return 0;
}

/**** PTRACE ****/

long sys_ptrace(enum __ptrace_request op, pid_t pid, void *addr, void *data) {
    return ptrace_handle(op, pid, addr, data);
}

/**** MLIBC ****/


long sys_read_entries(int handle, void *buffer, size_t max_size) {
    if (!FD_VALIDATE(handle)) return -EBADF;
    SYSCALL_VALIDATE_PTR_SIZE(buffer, max_size);

    vfs_file_t *f = FD(handle);

    vfs_dir_context_t ctx = {
        .dirpos = f->pos
    };

    unsigned char *p = (unsigned char*)buffer;
    size_t read = 0;
    while (read + sizeof(struct dirent) <= max_size) {
        int r = file_get_entries(f, &ctx);
        if (r == 1) {
            break;
        }

        if (r != 0) {
            return r;
        }

        ctx.dirpos++;
        f->pos++;

        struct dirent *ent = (struct dirent*)p;
        strncpy(ent->d_name, ctx.name, 1024);
        
        ent->d_ino = ctx.ino;
        ent->d_type = ctx.type;
        ent->d_reclen = sizeof(struct dirent);

        p += sizeof(struct dirent);
        read += sizeof(struct dirent);
    }  

    return read;
}
