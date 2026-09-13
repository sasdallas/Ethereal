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
    /* gap */
    [SYS_POLL]              = (syscall_func_t)(uintptr_t)sys_poll,
    [SYS_MKDIR]             = (syscall_func_t)(uintptr_t)sys_mkdir,
    [SYS_PSELECT]           = (syscall_func_t)(uintptr_t)sys_pselect,
    [SYS_READLINK]          = (syscall_func_t)(uintptr_t)sys_readlink,
    [SYS_ACCESS]            = (syscall_func_t)(uintptr_t)sys_access,
    /* gap */
    [SYS_FCNTL]             = (syscall_func_t)(uintptr_t)sys_fcntl,
    [SYS_UNLINKAT]          = (syscall_func_t)(uintptr_t)sys_unlinkat,
    [SYS_FTRUNCATE]         = (syscall_func_t)(uintptr_t)sys_ftruncate,
    /* gap */
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
    /* gap x2 */
    [SYS_EPOLL_CREATE]      = (syscall_func_t)(uintptr_t)NULL,
    [SYS_EPOLL_CTL]         = (syscall_func_t)(uintptr_t)NULL,
    [SYS_EPOLL_PWAIT]       = (syscall_func_t)(uintptr_t)NULL,
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
    [SYS_FCHMODAT]          = (syscall_func_t)(uintptr_t)sys_fchmodat,
    [SYS_MKNODAT]           = (syscall_func_t)(uintptr_t)0xdeadbeef,
    [SYS_FLOCK]             = (syscall_func_t)(uintptr_t)sys_flock,
    [SYS_UMASK]             = (syscall_func_t)(uintptr_t)sys_umask,
    [SYS_CLOCK_GETTIME]     = (syscall_func_t)(uintptr_t)sys_clock_gettime,
    [SYS_FSYNC]             = (syscall_func_t)(uintptr_t)sys_fsync,
    [SYS_PREAD]             = (syscall_func_t)(uintptr_t)sys_pread,
    [SYS_PWRITE]            = (syscall_func_t)(uintptr_t)sys_pwrite,
    [SYS_GETRLIMIT]         = (syscall_func_t)(uintptr_t)0xdeadbeef,
    [SYS_SETRLIMIT]         = (syscall_func_t)(uintptr_t)0xdeadbeef,
    [SYS_PAUSE]             = (syscall_func_t)(uintptr_t)sys_pause,
    [SYS_FCHOWNAT]          = (syscall_func_t)(uintptr_t)sys_fchownat,
    [SYS_FACCESSAT]         = (syscall_func_t)(uintptr_t)sys_faccessat,
    [SYS_SYNC]              = (syscall_func_t)(uintptr_t)sys_sync,
    [SYS_FSTATAT]           = (syscall_func_t)(uintptr_t)sys_fstatat,
    [SYS_SETGSBASE]         = (syscall_func_t)(uintptr_t)sys_setgsbase,
    [SYS_SIGRETURN]         = (syscall_func_t)(uintptr_t)sys_sigreturn,
    [SYS_SIGALTSTACK]       = (syscall_func_t)(uintptr_t)sys_sigaltstack
}; 


/* Unimplemented system call */
#define SYSCALL_UNIMPLEMENTED(syscall) ({ LOG(ERR, "[UNIMPLEMENTED] The system call \"%s\" is unimplemented\n", syscall); return 0; })

/* Log method */
#define LOG(status, ...) dprintf_module(status, "TASK:SYSCALL", __VA_ARGS__)

extern void syscall_trace_enter(syscall_t *syscall);
extern void syscall_trace_exit(syscall_t *syscall) ;


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
        // TODO: make this an actual system call and not just, whatever it is.
        current_cpu->current_process->flags |= PROCESS_TRACE_SYS;
        syscall->return_value = 0;
        return;
    }

    // Is the system call within bounds?
    if (syscall->syscall_number < 0 || syscall->syscall_number >= (int)(sizeof(syscall_table) / sizeof(*syscall_table))) {
        LOG(ERR, "Invalid system call %d received\n", syscall->syscall_number);
        syscall->return_value = -EINVAL;
        return;
    }

    if (current_cpu->current_process->flags & PROCESS_TRACE_SYS) {
        syscall_trace_enter(syscall);
    }

    // Call!
    syscall->return_value = (syscall_table[syscall->syscall_number])(
                                syscall->parameters[0], syscall->parameters[1], syscall->parameters[2],
                                syscall->parameters[3], syscall->parameters[4]);

                
#if 0
    if ((long)syscall->return_value < 0 && syscall->return_value != -EWOULDBLOCK && syscall->return_value != -ENODEV) {
        LOG(WARN, "system call %d failed with error %d\n", syscall->syscall_number, syscall->return_value);
    }
#endif

    if (current_cpu->current_process->flags & PROCESS_TRACE_SYS) {
        syscall_trace_exit(syscall);
    }

    return;
}

