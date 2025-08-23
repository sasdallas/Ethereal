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
#include <kernel/fs/vfs.h>
#include <kernel/fs/pipe.h>
#include <kernel/fs/pty.h>
#include <kernel/misc/args.h>
#include <kernel/mem/alloc.h>
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
    [SYS_READDIR]           = (syscall_func_t)(uintptr_t)sys_readdir,
    [SYS_POLL]              = (syscall_func_t)(uintptr_t)sys_poll,
    [SYS_MKDIR]             = (syscall_func_t)(uintptr_t)sys_mkdir,
    [SYS_PSELECT]           = (syscall_func_t)(uintptr_t)sys_pselect,
    [SYS_READLINK]          = (syscall_func_t)(uintptr_t)sys_readlink,
    [SYS_ACCESS]            = (syscall_func_t)(uintptr_t)sys_access,
    [SYS_CHMOD]             = (syscall_func_t)(uintptr_t)sys_chmod,
    [SYS_FCNTL]             = (syscall_func_t)(uintptr_t)sys_fcntl,
    [SYS_UNLINK]            = (syscall_func_t)(uintptr_t)sys_unlink,
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
}; 


/* Unimplemented system call */
#define SYSCALL_UNIMPLEMENTED(syscall) ({ LOG(ERR, "[UNIMPLEMENTED] The system call \"%s\" is unimplemented\n", syscall); return 0; })

/* Log method */
#define LOG(status, ...) dprintf_module(status, "TASK:SYSCALL", __VA_ARGS__)

/* Hostname */
/* TODO: Maybe move */
static char __hostname[256] = { 0 };
static size_t __hostnamelen = 0;

/**
 * @brief Pointer validation failed
 * @param ptr The pointer that failed to validate
 * @returns Only if resolved.
 */
void syscall_pointerValidateFailed(void *ptr) {
    // Check to see if this pointer is within process heap boundary
    if ((uintptr_t)ptr >= current_cpu->current_process->heap_base && (uintptr_t)ptr < current_cpu->current_process->heap) {
        // Yep, it's valid. Map a page
        mem_allocatePage(mem_getPage(NULL, (uintptr_t)ptr, MEM_CREATE), MEM_DEFAULT);
        return;
    }

    // Can we resolve via VAS?
    if (vas_fault(current_cpu->current_process->vas, (uintptr_t)(ptr) & ~0xFFF, PAGE_SIZE*2)) {
        return;
    }

    kernel_panic_prepare(KERNEL_BAD_ARGUMENT_ERROR);

    printf("*** Process \"%s\" tried to access an invalid pointer (%p)\n", current_cpu->current_process->name, ptr);
    dprintf(NOHEADER, COLOR_CODE_RED_BOLD "*** Process \"%s\" tried to access an invalid pointer (%p)\n\n" COLOR_CODE_RESET, current_cpu->current_process->name, ptr);

    kernel_panic_finalize();
}


/**
 * @brief Handle a system call
 * @param syscall The system call to handle
 * @returns Nothing, but updates @c syscall->return_value
 */
void syscall_handle(syscall_t *syscall) {
    // LOG(INFO, "Received system call %d\n", syscall->syscall_number);

    // Enter
    ptrace_event(PROCESS_TRACE_SYSCALL);

    // Is the system call within bounds?
    if (syscall->syscall_number < 0 || syscall->syscall_number >= (int)(sizeof(syscall_table) / sizeof(*syscall_table))) {
        LOG(ERR, "Invalid system call %d received\n");
        syscall->return_value = -EINVAL;
        return;
    }

    // Call!
    syscall->return_value = (syscall_table[syscall->syscall_number])(
                                syscall->parameters[0], syscall->parameters[1], syscall->parameters[2],
                                syscall->parameters[3], syscall->parameters[4]);

    // Exit
    ptrace_event(PROCESS_TRACE_SYSCALL);

    return;
}

void sys_exit(int status) {
    LOG(DEBUG, "process %s sys_exit %d\n", current_cpu->current_process->name, status);
    process_exit(NULL, status);
}

int sys_open(const char *pathname, int flags, mode_t mode) {
    // Validate pointer
    SYSCALL_VALIDATE_PTR(pathname);
    LOG(DEBUG, "sys_open %s flags %d mode %d\n", pathname, flags, mode);

    // !!!: HACK
    if (!strcmp(pathname, "/dev/ptmx")) {
        // Make a PTY
        pty_t *pty = pty_create(NULL, NULL, -1);
        
        // Create file descriptors
        fd_t *master_fd = fd_add(current_cpu->current_process, pty->master);
        return master_fd->fd_number;
    }

    // Try and get it open
    fs_node_t *node = kopen_user(pathname, flags);

    // Did we find the node and they DIDN'T want us to create it?
    if (node && (flags & O_CREAT) && (flags & O_EXCL)) {
        fs_close(node);
        return -EEXIST;
    }

    // Did we find it and did they want to create it?
    if (!node && (flags & O_CREAT)) {
        // Ok, make the file using some garbage hacks
        int ret = vfs_creat(&node, (char*)pathname, mode);
        if (ret < 0) {
            return ret;
        }

        // HACK: Open the node
        ret = fs_open(node, flags);
        if (ret != 0) {
            return ret;
        }
    }

    // Did they want a directory?
    if (node && !(node->flags & VFS_DIRECTORY) && (flags & O_DIRECTORY)) {
        fs_close(node);
        return -ENOTDIR;
    }

    // Did we find it and they want it?
    if (!node) {
        return -ENOENT;
    }

    // Create the file descriptor and return
    fd_t *fd = fd_add(current_cpu->current_process, node);
    
    // Are they trying to append? If so modify length to be equal to node length
    if (flags & O_APPEND) {
        fd->offset = node->length;
    }

    return fd->fd_number;
}

ssize_t sys_read(int fd, void *buffer, size_t count) {
    SYSCALL_VALIDATE_PTR_SIZE(buffer, count);

    if (!FD_VALIDATE(current_cpu->current_process, fd)) {
        return -EBADF;
    }

    fd_t *proc_fd = FD(current_cpu->current_process, fd);

    // LOG(DEBUG, "sys_read fd %d buffer %p count %d offset %d\n", fd, buffer, count, proc_fd->offset);
    ssize_t i = fs_read(proc_fd->node, proc_fd->offset, count, (uint8_t*)buffer);
    proc_fd->offset += i;

    return i;
}

ssize_t sys_write(int fd, const void *buffer, size_t count) {
    SYSCALL_VALIDATE_PTR_SIZE(buffer, count);
    
    if (!FD_VALIDATE(current_cpu->current_process, fd)) {
        return -EBADF;
    }

    fd_t *proc_fd = FD(current_cpu->current_process, fd);
    ssize_t i = fs_write(proc_fd->node, proc_fd->offset, count, (uint8_t*)buffer);
    proc_fd->offset += i;

    // LOG(DEBUG, "sys_write fd %d buffer %p count %d\n", fd, buffer, count);
    if (!i) LOG(WARN, "sys_write wrote nothing for size %d\n", count);
    return i;
}

int sys_close(int fd) {
    if (!FD_VALIDATE(current_cpu->current_process, fd)) {
        LOG(WARN, "Bad file descriptor close attempt on fd %d\n", fd);
        return -EBADF;
    }

    // // !!! HACK: If you don't want to free this, too bad...
    // if (FD(current_cpu->current_process, fd)->dev) kfree(FD(current_cpu->current_process, fd)->dev);

    // LOG(DEBUG, "sys_close fd %d\n", fd);
    fd_remove(current_cpu->current_process, fd);
    return 0;
}

/**
 * @brief Common stat for stat/fstat/lstat
 * @param f The file to check
 * @param statbuf The stat buffer to use
 */
static void sys_stat_common(fs_node_t *f, struct stat *statbuf) {
    // Convert VFS flags to st_dev
    statbuf->st_dev = 0;
    if (f->flags == VFS_DIRECTORY)      statbuf->st_dev |= S_IFDIR; // Directory
    if (f->flags == VFS_BLOCKDEVICE)    statbuf->st_dev |= S_IFBLK; // Block device
    if (f->flags == VFS_CHARDEVICE)     statbuf->st_dev |= S_IFCHR; // Character device
    if (f->flags == VFS_FILE)           statbuf->st_dev |= S_IFREG; // Regular file
    if (f->flags == VFS_SYMLINK)        statbuf->st_dev |= S_IFLNK; // Symlink
    if (f->flags == VFS_PIPE)           statbuf->st_dev |= S_IFIFO; // FIFO or not, it's a pipe
    if (f->flags == VFS_SOCKET)         statbuf->st_dev |= S_IFSOCK; // Socket
    if (f->flags == VFS_MOUNTPOINT)     statbuf->st_dev |= S_IFDIR; // ???

    // st_mode is just st_dev with extra steps
    statbuf->st_mode = statbuf->st_dev;

    // Setup other fields
    statbuf->st_ino = f->inode; // Inode number
    statbuf->st_mode |= f->mask; // File mode - TODO: Make sure that file mode is properly set with vaild mask bits
    statbuf->st_nlink = 0; // TODO
    statbuf->st_uid = f->uid;
    statbuf->st_gid = f->gid;
    statbuf->st_rdev = 0; // TODO
    statbuf->st_size = f->length;
    statbuf->st_blksize = 512; // TODO: This would prove useful for file I/O
    statbuf->st_blocks = 0; // TODO
    statbuf->st_atime = f->atime;
    statbuf->st_mtime = f->mtime;
    statbuf->st_ctime = f->ctime;
}

long sys_stat(const char *pathname, struct stat *statbuf) {
    SYSCALL_VALIDATE_PTR(pathname);
    SYSCALL_VALIDATE_PTR(statbuf);

    // Try to open the file
    fs_node_t *f = kopen_user(pathname, O_RDONLY); // TODO: return ELOOP, O_NOFOLLOW is supposed to work but need to refine this
    if (!f) return -ENOENT;

    // Common stat
    sys_stat_common(f, statbuf);

    // Close the file
    fs_close(f);

    // Done
    return 0;
}

long sys_fstat(int fd, struct stat *statbuf) {
    if (!FD_VALIDATE(current_cpu->current_process, fd)) return -EBADF;
    SYSCALL_VALIDATE_PTR(statbuf);

    // Try to do stat
    sys_stat_common(FD(current_cpu->current_process, fd)->node, statbuf);
    return 0;
}

/**
 * @brief lstat system call
 */
long sys_lstat(const char *pathname, struct stat *statbuf) {
    SYSCALL_VALIDATE_PTR(pathname);
    SYSCALL_VALIDATE_PTR(statbuf);

    // Try to open the file
    fs_node_t *f = kopen_user(pathname, O_NOFOLLOW | O_PATH);   // Get actual link file
                                                                // TODO: Handle open errors?
    
    if (!f) return -ENOENT;

    // Common stat
    sys_stat_common(f, statbuf);

    // Close the file
    fs_close(f);

    // Done
    return 0;
}

/**
 * @brief ioctl
 */
long sys_ioctl(int fd, unsigned long request, void *argp) {
    if (!FD_VALIDATE(current_cpu->current_process, fd)) return -EBADF;
    
    // Do not validate argp
    return fs_ioctl(FD(current_cpu->current_process, fd)->node, request, argp);
}

/**
 * @brief Change data segment size
 */
void *sys_brk(void *addr) {
    LOG(DEBUG, "sys_brk addr %p (curheap: %p - %p)\n", addr, current_cpu->current_process->heap_base, current_cpu->current_process->heap);

    // Validate pointer is in range
    if ((uintptr_t)addr < current_cpu->current_process->heap_base) {
        // brk() syscall should return current program heap location on error
        return (void*)current_cpu->current_process->heap;
    }

    // TODO: Validate resource limit

    // If the user wants to shrink the heap, then do it
    if ((uintptr_t)addr < current_cpu->current_process->heap) {
        // TODO: Free area in VAS!!!
        size_t free_size = current_cpu->current_process->heap - (uintptr_t)addr;
        mem_free((uintptr_t)addr, free_size, MEM_DEFAULT);
        current_cpu->current_process->heap = (uintptr_t)addr;
        return addr;
    } else if ((uintptr_t)addr == current_cpu->current_process->heap) {
        return addr;
    }


    // Else, "handle"
    vas_reserve(current_cpu->current_process->vas, current_cpu->current_process->heap, (uintptr_t)addr - current_cpu->current_process->heap, VAS_ALLOC_PROG_BRK);
    
    current_cpu->current_process->heap = (uintptr_t)addr;   // Sure.. you can totally have this memory ;)
                                                            // (page fault handler will map this on a critical failure)


    return addr;
}

/**
 * @brief Fork system call
 */
pid_t sys_fork() {
    return process_fork();
}

/**
 * @brief lseek system call
 */
off_t sys_lseek(int fd, off_t offset, int whence) {
    if (!FD_VALIDATE(current_cpu->current_process, fd)) {
        return -EBADF;
    }

    // Handle whence
    if (whence == SEEK_SET) {
        FD(current_cpu->current_process, fd)->offset = offset;
    } else if (whence == SEEK_CUR) {
        FD(current_cpu->current_process, fd)->offset += offset;
    } else if (whence == SEEK_END) {
        // TODO: This is the problem area (offset > node length)
        FD(current_cpu->current_process, fd)->offset = FD(current_cpu->current_process, fd)->node->length + offset;
    } else {
        return -EINVAL;
    }


    // TODO: What if offset > file size? We don't have proper safeguards, Linux does something but I didn't care enough to check...
    return FD(current_cpu->current_process, fd)->offset;
}

/**
 * @brief Get time of day system call
 * @todo Use struct timezone
 */
long sys_gettimeofday(struct timeval *tv, void *tz) {
    SYSCALL_VALIDATE_PTR(tv);
    if (tz) SYSCALL_VALIDATE_PTR(tz);
    return gettimeofday(tv, tz);
}

/**
 * @brief Set time of day system call
 * @todo Use struct timezone
 */
long sys_settimeofday(struct timeval *tv, void *tz) {
    SYSCALL_VALIDATE_PTR(tv);
    SYSCALL_VALIDATE_PTR(tz);
    return settimeofday(tv, tz);
}

/**
 * @brief usleep system call
 */
long sys_usleep(useconds_t usec) {
    if (usec < 10000) {
        // !!!: knock it off, this will crash.
        return 0;
    }

    sleep_untilTime(current_cpu->current_thread, (usec / 1000000), (usec % 1000000));
    if (sleep_enter() == WAKEUP_SIGNAL) return -EINTR;

    return 0;
}


/**
 * @brief execve system call
 */
long sys_execve(const char *pathname, const char *argv[], const char *envp[]) {
    // Validate pointers
    SYSCALL_VALIDATE_PTR(pathname);
    SYSCALL_VALIDATE_PTR(argv);
    if (envp) SYSCALL_VALIDATE_PTR(envp);

    // Try to get the file
    fs_node_t *f = kopen_user(pathname, O_RDONLY);
    if (!f) return -ENOENT;
    if (f->flags != VFS_FILE && f->flags != VFS_SYMLINK) { fs_close(f); return -EISDIR; }

    // Collect any arguments that we need
    int argc = 0;
    int envc = 0;

    // Collect argc
    while (argv[argc]) {
        // TODO: Problem?
        SYSCALL_VALIDATE_PTR(argv[argc]);
        argc++;
    }

    // Collect envc
    if (envp) {
        while (envp[envc]) {
            SYSCALL_VALIDATE_PTR(envp[envc]);
            envc++;
        }
    }

    // Move their arguments into our array
    char **new_argv = kzalloc((argc+1) * sizeof(char*));
    for (int a = 0; a < argc; a++) {
        new_argv[a] = strdup(argv[a]); // TODO: Leaking memory!!!
    }

    // Reallocate envp if specified
    char **new_envp = kzalloc((envc+1) * sizeof(char*));
    if (envp) {
        for (int e = 0; e < envc; e++) {
            new_envp[e] = strdup(envp[e]); // TODO: Leaking memory!!!
        }
    } 

    // Set null terminators
    new_argv[argc] = NULL;
    new_envp[envc] = NULL;

    // process_execute(f, argc, new_argv, new_envp);
    return binfmt_exec((char*)pathname, f, argc, new_argv, new_envp);
}

/**
 * @brief wait system call
 */
long sys_wait(pid_t pid, int *wstatus, int options) {
    if (wstatus) {
        SYSCALL_VALIDATE_PTR(wstatus);
    }

    return process_waitpid(pid, wstatus, options);
}

/**
 * @brief getcwd system call
 */
long sys_getcwd(char *buffer, size_t size) {
    if (!size || !buffer) return 0;
    SYSCALL_VALIDATE_PTR(buffer)

    // Copy the current working directory to buffer
    size_t wd_len = strlen(current_cpu->current_process->wd_path);
    strncpy(buffer, current_cpu->current_process->wd_path, (wd_len > size) ? size : wd_len);
    return size;
}

/**
 * @brief chdir system call
 */
long sys_chdir(const char *path) {
    SYSCALL_VALIDATE_PTR(path);

    // Check that the path is accessible
    char *new_path = vfs_canonicalizePath(current_cpu->current_process->wd_path, (char*)path);
    char *nn = strdup(new_path); // TEMPORARY

    fs_node_t *tmpnode = kopen(new_path, O_RDONLY);
    if (tmpnode) {
        if (tmpnode->flags != VFS_DIRECTORY) {
            kfree(nn);
            fs_close(tmpnode);
            return -ENOTDIR;
        }
        
        // Path exists
        // TODO: Validate permissions?
        kfree(current_cpu->current_process->wd_path);
        current_cpu->current_process->wd_path = nn;

        // Close node
        fs_close(tmpnode);
        return 0;
    }

    kfree(nn);
    return -ENOENT;
}

/**
 * @brief fchdir system call
 */
long sys_fchdir(int fd) {
    SYSCALL_UNIMPLEMENTED("sys_fchdir");
    return 0;
}

/**
 * @brief readdir system call
 */
long sys_readdir(struct dirent *ent, int fd, unsigned long index) {
    SYSCALL_VALIDATE_PTR(ent);
    if (!FD_VALIDATE(current_cpu->current_process, fd)) {
        return -EBADF;
    }

    struct dirent *dent = fs_readdir(FD(current_cpu->current_process, fd)->node, index);
    if (!dent) return 0; // EOF

    memcpy(ent, dent, sizeof(struct dirent));
    kfree(dent);
    return 1;
}

long sys_poll(struct pollfd fds[], nfds_t nfds, int timeout) {
    if (!nfds) return 0;
    SYSCALL_VALIDATE_PTR_SIZE(fds, sizeof(struct pollfd) * nfds);

    int have_hit = 0;

    /* !!!: Currently broken */
#ifdef POLL_USES_FS_WAIT

    for (size_t i = 0; i < nfds; i++) {
        // Check the file descriptor
        fds[i].revents = 0;
        if (!FD_VALIDATE(current_cpu->current_process, fds[i].fd)) {
            fds[i].revents |= POLLNVAL;
            continue;
        }

        // Does the file descriptor have available contents right now?
        int events = ((fds[i].events & POLLIN) ? VFS_EVENT_READ : 0) | ((fds[i].events & POLLOUT) ? VFS_EVENT_WRITE : 0);
        int ready = fs_ready(FD(current_cpu->current_process, fds[i].fd)->node, events);

        if (ready & events) {
            // Oh, we already have a hit! :D
            // LOG(DEBUG, "Hit on file descriptor %d for events %s %s\n", fds[i].fd, (ready & VFS_EVENT_READ) ? "VFS_EVENT_READ" : "", (ready & VFS_EVENT_WRITE) ? "VFS_EVENT_WRITE" : "");
            fds[i].revents = (events & VFS_EVENT_READ && ready & VFS_EVENT_READ) ? POLLIN : 0 | (events & VFS_EVENT_WRITE && ready & VFS_EVENT_WRITE) ? POLLOUT : 0;
            have_hit++;
        } 
    }

    if (have_hit) return have_hit;

    // We didn't get anything. Did they want us to wait?
    if (timeout == 0) return 0;
    
    // Yes, so prepare ourselves to wait
    if (timeout > 0) {
        sleep_untilTime(current_cpu->current_thread, 0, timeout*1000);
    } else {
        sleep_untilNever(current_cpu->current_thread);
    }

    // There is a timeout, so put ourselves in the queue for each fd
    for (size_t i = 0; i < nfds; i++) {
        int events = ((fds[i].events & POLLIN) ? VFS_EVENT_READ : 0) | ((fds[i].events & POLLOUT) ? VFS_EVENT_WRITE : 0);
        if (FD_VALIDATE(current_cpu->current_process, fds[i].fd)) fs_wait(FD(current_cpu->current_process, fds[i].fd)->node, events);
    }

    // Enter sleep state
    int wakeup = sleep_enter();

    if (wakeup == WAKEUP_SIGNAL) return -EINTR;
    if (wakeup == WAKEUP_TIME) return 0;

    for (size_t i = 0; i < nfds; i++) {
        // Does the file descriptor have available contents right now?
        if (!FD_VALIDATE(current_cpu->current_process, fds[i].fd)) continue;
        int events = ((fds[i].events & POLLIN) ? VFS_EVENT_READ : 0) | ((fds[i].events & POLLOUT) ? VFS_EVENT_WRITE : 0);
        int ready = fs_ready(FD(current_cpu->current_process, fds[i].fd)->node, events);

        if (ready & events) {
            // LOG(DEBUG, "Hit on file descriptor %d for events %s %s\n", fds[i].fd, (ready & VFS_EVENT_READ) ? "VFS_EVENT_READ" : "", (ready & VFS_EVENT_WRITE) ? "VFS_EVENT_WRITE" : "");
            fds[i].revents = (events & VFS_EVENT_READ && ready & VFS_EVENT_READ) ? POLLIN : 0 | (events & VFS_EVENT_WRITE && ready & VFS_EVENT_WRITE) ? POLLOUT : 0;
            have_hit++;
        } 
    }

    return have_hit;   // At least one thread woke us up

#else

    struct timeval tv_start;
    gettimeofday(&tv_start, NULL);

    while (1) {
        for (size_t i = 0; i < nfds; i++) {
            // Check the file descriptor
            fds[i].revents = 0;
            if (!FD_VALIDATE(current_cpu->current_process, fds[i].fd)) {
                fds[i].revents |= POLLNVAL;
                continue;
            }

            // Does the file descriptor have available contents right now?
            int events = ((fds[i].events & POLLIN) ? VFS_EVENT_READ : 0) | ((fds[i].events & POLLOUT) ? VFS_EVENT_WRITE : 0);
            int ready = fs_ready(FD(current_cpu->current_process, fds[i].fd)->node, events);

            if (ready & events) {
                // Oh, we already have a hit! :D
                // LOG(DEBUG, "Hit on file descriptor %d for events %s %s\n", fds[i].fd, (ready & VFS_EVENT_READ) ? "VFS_EVENT_READ" : "", (ready & VFS_EVENT_WRITE) ? "VFS_EVENT_WRITE" : "");
                fds[i].revents = (events & VFS_EVENT_READ && ready & VFS_EVENT_READ) ? POLLIN : 0 | (events & VFS_EVENT_WRITE && ready & VFS_EVENT_WRITE) ? POLLOUT : 0;
                have_hit++;
            } 
        }

        if (have_hit) return have_hit;

        // We didn't get anything. Did they want us to wait?
        if (timeout == 0) return 0;

        struct timeval tv;
        gettimeofday(&tv, NULL);
        
        if ((tv.tv_sec*1000) - (tv_start.tv_sec*1000) + (tv.tv_usec/1000) > timeout && timeout != -1) {
            return 0;
        }

        process_yield(1);
    }
#endif

}

long sys_pselect(sys_pselect_context_t *ctx) {
    if (ctx->readfds) SYSCALL_VALIDATE_PTR(ctx->readfds);
    if (ctx->writefds) SYSCALL_VALIDATE_PTR(ctx->writefds);
    if (ctx->errorfds) SYSCALL_VALIDATE_PTR(ctx->errorfds);
    if (ctx->timeout) SYSCALL_VALIDATE_PTR(ctx->timeout);
    if (ctx->sigmask) SYSCALL_VALIDATE_PTR(ctx->sigmask);

    sigset_t old_set = current_cpu->current_thread->blocked_signals;

    if (ctx->sigmask) {
        current_cpu->current_thread->blocked_signals = *(ctx->sigmask);
    }

    // Create the return sets
    fd_set rfds, wfds, efds;
    FD_ZERO(&rfds);
    FD_ZERO(&wfds);
    FD_ZERO(&efds);

    size_t ret = 0;

    // Depending on our timeval, prepare the thread for sleeping
    if (ctx->timeout) {
        sleep_untilTime(current_cpu->current_thread, ctx->timeout->tv_sec, ctx->timeout->tv_nsec / 1000); // TODO: tv_nsec
    } else {
        sleep_untilNever(current_cpu->current_thread);
    }

    // First check if anything is reaady
    for (int fd = 0; fd < ctx->nfds; fd++) {
        if (!((ctx->readfds && FD_ISSET(fd, ctx->readfds)) || (ctx->writefds && FD_ISSET(fd, ctx->writefds)) || (ctx->errorfds && FD_ISSET(fd, ctx->errorfds)))) {
            continue; // No need to care, it isn't a wanted file descriptor
        }

        // Validate file descriptor
        if (!FD_VALIDATE(current_cpu->current_process, fd)) continue;


        // Get events
        int ev = fs_ready(FD(current_cpu->current_process, fd)->node, VFS_EVENT_READ | VFS_EVENT_WRITE | VFS_EVENT_ERROR);

        // Check
        if (ctx->readfds && FD_ISSET(fd, ctx->readfds) && (ev & VFS_EVENT_READ)) {
            // Read 
            FD_SET(fd, &rfds);
            ret++;
        }

        if (ctx->writefds && FD_ISSET(fd, ctx->writefds) && (ev & VFS_EVENT_WRITE)) {
            // Write
            FD_SET(fd, &wfds);
            ret++;
        }

        if (ctx->errorfds && FD_ISSET(fd, ctx->errorfds) && (ev & VFS_EVENT_ERROR)) {
            // Error
            FD_SET(fd, &efds);
            ret++;
        } 
    }

    // Did we get anything?
    if (ret) {
        sleep_exit(current_cpu->current_thread);
        (current_cpu->current_thread->blocked_signals) = old_set;
        return ret;
    }

    // Nope, looks like we have to go on an adventure.

    // Now that we're prepped, go through each file descriptor one more time to wait in the queue
    for (int fd = 0; fd < ctx->nfds; fd++) {
        int wanted_evs = 0;
        if (ctx->readfds && FD_ISSET(fd, ctx->readfds)) wanted_evs |= VFS_EVENT_READ;
        if (ctx->writefds && FD_ISSET(fd, ctx->writefds)) wanted_evs |= VFS_EVENT_WRITE;
        if (ctx->errorfds && FD_ISSET(fd, ctx->errorfds)) wanted_evs |= VFS_EVENT_ERROR;
        
        if (!FD_VALIDATE(current_cpu->current_process, fd)) continue;

        // Now wait in the node
        fs_wait(FD(current_cpu->current_process, fd)->node, wanted_evs);
    }

    // Enter sleep
    int w = sleep_enter();
    if (w == WAKEUP_SIGNAL) {
        (current_cpu->current_thread->blocked_signals) = old_set;
        return -EINTR; // TODO: SA_RESTART
    }

    if (w == WAKEUP_TIME) {
        (current_cpu->current_thread->blocked_signals) = old_set;
        return 0;
    }



    // Another thread must have woken us up
    for (int fd = 0; fd < ctx->nfds; fd++) {
        if (!((ctx->readfds && FD_ISSET(fd, ctx->readfds)) || (ctx->writefds && FD_ISSET(fd, ctx->writefds)) || (ctx->errorfds && FD_ISSET(fd, ctx->errorfds)))) {
            continue; // No need to care, it isn't a wanted file descriptor
        }

        // Validate file descriptor
        if (!FD_VALIDATE(current_cpu->current_process, fd)) continue;

        // Get events
        int ev = fs_ready(FD(current_cpu->current_process, fd)->node, VFS_EVENT_READ | VFS_EVENT_WRITE | VFS_EVENT_ERROR);

        // Check
        if (ctx->readfds && FD_ISSET(fd, ctx->readfds) && (ev & VFS_EVENT_READ)) {
            // Read 
            FD_SET(fd, &rfds);
            ret++;
        }

        if (ctx->writefds && FD_ISSET(fd, ctx->writefds) && (ev & VFS_EVENT_WRITE)) {
            // Write
            FD_SET(fd, &wfds);
            ret++;
        }

        if (ctx->errorfds && FD_ISSET(fd, ctx->errorfds) && (ev & VFS_EVENT_ERROR)) {
            // Error
            FD_SET(fd, &efds);
            ret++;
        } 
    }

    if (ctx->readfds) memcpy(ctx->readfds, &rfds, sizeof(fd_set));
    if (ctx->writefds) memcpy(ctx->writefds, &wfds, sizeof(fd_set));
    if (ctx->errorfds) memcpy(ctx->errorfds, &efds, sizeof(fd_set));

    (current_cpu->current_thread->blocked_signals) = old_set;

    // Ready!
    return ret; 
}

ssize_t sys_readlink(const char *path, char *buf, size_t bufsiz) {
    LOG(ERR, "sys_readlink is unimplemented (%s)\n", path);
    return -EINVAL;
}

long sys_access(const char *path, int amode) {
    SYSCALL_VALIDATE_PTR(path);

    int flags = 0;

    // TODO: F_OK and X_OK
    if (amode & R_OK) flags |= O_RDONLY;
    if (amode & W_OK) flags |= O_WRONLY;

    // TODO: Better this.. vfs_errno?
    fs_node_t *n = kopen_user(path, amode);
    if (!n) return -ENOENT;
    fs_close(n);

    return 0;
}

long sys_chmod(const char *path, mode_t mode) {
    SYSCALL_UNIMPLEMENTED("sys_chmod");
}

long sys_fcntl(int fd, int cmd, int extra) {
    SYSCALL_UNIMPLEMENTED("sys_fcntl");
}

long sys_unlink(const char *pathname) {
    SYSCALL_VALIDATE_PTR(pathname);
    LOG(INFO, "sys_unlink: %s: UNIMPLEMENTED\n", pathname);
    return -EROFS;
}

long sys_ftruncate(int fd, off_t length) {
    if (!FD_VALIDATE(current_cpu->current_process, fd)) return -EBADF;
    return fs_truncate(FD(current_cpu->current_process, fd)->node, length); 
}

long sys_mkdir(const char *pathname, mode_t mode) {
    SYSCALL_VALIDATE_PTR(pathname);
    return vfs_mkdir((char*)pathname, mode);
}


long sys_uname(struct utsname *buf) {
    SYSCALL_VALIDATE_PTR(buf);

    // Copy!
    snprintf(buf->sysname, 128, "Hexahedron");
    snprintf(buf->nodename, 128, "N/A"); // lol, should be hostname
    snprintf(buf->release, 128, "%d.%d.%d-%s", 
                    __kernel_version_major, 
                    __kernel_version_minor, 
                    __kernel_version_lower,
                    __kernel_build_configuration);

    snprintf(buf->version, 128, "%s %s %s",
        __kernel_version_codename,
        __kernel_build_date,
        __kernel_build_time);

    snprintf(buf->machine, 128, "%s", __kernel_architecture);

    return 0;
}

pid_t sys_getpid() {
    return current_cpu->current_process->pid;
}

/* MMAP */

long sys_mmap(sys_mmap_context_t *context) {
    SYSCALL_VALIDATE_PTR(context);
    LOG(DEBUG, "TRACE: sys_mmap %p %d %d %d %d %d\n", context->addr, context->len, context->prot,context->flags, context->filedes, context->off);
    return (long)process_mmap(context->addr, context->len, context->prot, context->flags, context->filedes, context->off);
}

long sys_munmap(void *addr, size_t len) {
    return process_munmap(addr, len);
}

long sys_msync(void *addr, size_t len, int flush) {
    LOG(WARN, "sys_msync %p %d %d\n");
    return 0;
}

long sys_mprotect(void *addr, size_t len, int prot) {
    LOG(WARN, "sys_mprotect %p %d %d\n", addr, len, prot);
    return 0;
}

/* TIMES */

clock_t sys_times(struct tms *buf) {
    return -ENOSYS;
}

/* DUP */

long sys_dup2(int oldfd, int newfd) {
    if (!FD_VALIDATE(current_cpu->current_process, oldfd)) {
        return -EBADF;
    }   

    if (newfd == -1) {
        // We assign!
        fd_t *fd = fd_add(current_cpu->current_process, FD(current_cpu->current_process, oldfd)->node);
        fd->mode = FD(current_cpu->current_process, oldfd)->mode;
        fd->offset = FD(current_cpu->current_process, oldfd)->offset;
        return fd->fd_number;
    }

    int err = fd_duplicate(current_cpu->current_process, oldfd, newfd);
    if (err != 0) return err;
    return newfd;
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
        if (oact->sa_handler == SIGNAL_ACTION_DEFAULT) oact->sa_handler = SIG_DFL;
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
        LOG(DEBUG, "Changed signal %s to use handler %p mask 0x%016llx flags 0x%x\n", strsignal(signum), act->sa_handler, act->sa_mask, act->sa_flags);
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
    return socket_sendmsg(socket, message, flags);
}

long sys_recvmsg(int socket, struct msghdr *message, int flags) {
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

    // Canonicalize paths
    char *src_canonicalized = vfs_canonicalizePath(current_cpu->current_process->wd_path, (char*)src);
    char *dst_canonicalized = vfs_canonicalizePath(current_cpu->current_process->wd_path, (char*)dst);


    // Try to mount filesystem type
    fs_node_t *node = NULL;
    int success = vfs_mountFilesystemType((char*)type, (char*)src_canonicalized, (char*)dst_canonicalized, &node);
    kfree(src_canonicalized);
    kfree(dst_canonicalized);

    // Success?
    if (success != 0) {
        return success;
    }

    return 0;
}

long sys_umount(const char *mountpoint) {
    return -ENOTSUP;
}

/**** PIPES ****/

long sys_pipe(int fildes[2]) {
    SYSCALL_VALIDATE_PTR(fildes);

    return pipe_create(current_cpu->current_process, fildes);
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

/**** PTY ****/

long sys_openpty(int *amaster, int *aslave, char *name, const struct termios *termp, const struct winsize *winp) {
    if (termp) SYSCALL_VALIDATE_PTR(termp);
    if (winp) SYSCALL_VALIDATE_PTR(winp);
    SYSCALL_VALIDATE_PTR(amaster);
    SYSCALL_VALIDATE_PTR(aslave);

    // Make a PTY
    pty_t *pty = pty_create((struct termios*)termp, (struct winsize*)winp, -1);
    
    // Create file descriptors
    fd_t *master_fd = fd_add(current_cpu->current_process, pty->master);
    fd_t *slave_fd = fd_add(current_cpu->current_process, pty->slave);
    
    // Set values
    *amaster = master_fd->fd_number;
    *aslave = slave_fd->fd_number;

    // Open the file descriptors too
    fs_open(pty->master, 0);
    fs_open(pty->slave, 0);

    return 0;
}

/**** IDS ****/

uid_t sys_getuid() {
    return current_cpu->current_process->uid;
}

int sys_setuid(uid_t uid) {
    if (PROC_IS_ROOT(current_cpu->current_process)) {
        current_cpu->current_process->uid = uid;
        current_cpu->current_process->euid = uid;
        return 0;
    } 

    return -EPERM;
}

gid_t sys_getgid() {
    return current_cpu->current_process->gid;
}

int sys_setgid(gid_t gid) {
    if (PROC_IS_ROOT(current_cpu->current_process)) {
        current_cpu->current_process->gid = gid;
        current_cpu->current_process->egid = gid;
        return 0;
    }

    return -EPERM;
}

pid_t sys_getppid() {
    if (current_cpu->current_process->parent) {
        return current_cpu->current_process->parent->pid;
    }

    return 0;
}

pid_t sys_getpgid(pid_t pid) {
    if (pid < 0) return -EINVAL;
    if (!pid) return current_cpu->current_process->pgid;
    
    process_t *p = process_getFromPID(pid);
    if (!p) return -ESRCH;

    return p->pgid;
}

int sys_setpgid(pid_t pid, pid_t pgid) {
    if (pid < 0) return -EINVAL;
    
    process_t *p = current_cpu->current_process;

    if (pid) {
        p = process_getFromPID(pid);
        if (!p) return -ESRCH;
    }

    if (p->sid != current_cpu->current_process->sid || p->sid == p->pid) {
        return -EPERM; // Attempt to change process in a different session or session leader
    }

    if (!pgid) {
        p->pgid = p->pid;
    } else {
        // Validate PGID
        process_t *valid = process_getFromPID(pgid);
        if (!valid || valid->sid != p->sid) return -EPERM;
        
        p->pgid = pgid;
    }

    return 0;
}

pid_t sys_getsid() {
    return current_cpu->current_process->sid;
}

pid_t sys_setsid() {
    if (current_cpu->current_process->sid == current_cpu->current_process->pid) return -EPERM;
    current_cpu->current_process->sid = current_cpu->current_process->pid;
    return 0;
}

uid_t sys_geteuid() {
    return current_cpu->current_process->euid;
}

int sys_seteuid(uid_t uid) {
    if (!PROC_IS_ROOT(current_cpu->current_process) && uid != current_cpu->current_process->uid) {
        // Nope
        return -EPERM;
    }

    current_cpu->current_process->euid = uid;
    return 0;
}

gid_t sys_getegid() {
    return current_cpu->current_process->egid;
}

int sys_setegid(gid_t gid) {
    if (!PROC_IS_ROOT(current_cpu->current_process) && gid != current_cpu->current_process->gid) {
        return -EPERM;
    }

    current_cpu->current_process->egid = gid;
    return 0;
}

/**** HOSTNAMES ****/

long sys_gethostname(char *name, size_t size) {
    SYSCALL_VALIDATE_PTR_SIZE(name, size);

    memcpy(name, __hostname, (size > __hostnamelen) ? __hostnamelen : size);

    if (size < __hostnamelen) return -ENAMETOOLONG;
    return 0;
}

long sys_sethostname(const char *name, size_t size) {
    if (!PROC_IS_ROOT(current_cpu->current_process)) return -EPERM;
    SYSCALL_VALIDATE_PTR_SIZE(name, size);
    if (size > 256) return -EINVAL;

    // Set the hostname
    memcpy(__hostname, name, size);
    __hostnamelen = size;
    
    return 0;
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
            ovalue->it_interval.tv_sec = current_cpu->current_process->itimers[which].reset_value.tv_sec;
            ovalue->it_interval.tv_usec = current_cpu->current_process->itimers[which].reset_value.tv_usec;
            ovalue->it_value.tv_sec = current_cpu->current_process->itimers[which].value.tv_usec;
            ovalue->it_value.tv_usec = current_cpu->current_process->itimers[which].value.tv_usec;
        }

        int r = timer_set(current_cpu->current_process, which, (struct itimerval*)value);
        if (r != 0) return r;
    } else {
        // They just want to get the timer in ovalue
        ovalue->it_interval.tv_sec = current_cpu->current_process->itimers[which].reset_value.tv_sec;
        ovalue->it_interval.tv_usec = current_cpu->current_process->itimers[which].reset_value.tv_usec;
        ovalue->it_value.tv_sec = current_cpu->current_process->itimers[which].value.tv_usec;
        ovalue->it_value.tv_usec = current_cpu->current_process->itimers[which].value.tv_usec;
    }

    return 0;
}

/**** PTRACE ****/

long sys_ptrace(enum __ptrace_request op, pid_t pid, void *addr, void *data) {
    return ptrace_handle(op, pid, addr, data);
}

/**** MLIBC ****/

struct readdir_context {
    int fd;
    int ent;
};

long sys_read_entries(int handle, void *buffer, size_t max_size) {
    if (!FD_VALIDATE(current_cpu->current_process, handle)) return -EBADF;
    SYSCALL_VALIDATE_PTR_SIZE(buffer, max_size);

    LOG(DEBUG, "TRACE: sys_read_entries %d %p %d\n", handle, buffer, max_size);

    fd_t *f = FD(current_cpu->current_process, handle);
    if (!f->dev) { f->dev = kzalloc(sizeof(struct readdir_context)); ((struct readdir_context*)f->dev)->fd = handle; }

    struct readdir_context *ctx = (struct readdir_context*)f->dev;

    unsigned char *p = (unsigned char*)buffer;
    size_t read = 0;
    while (read + sizeof(struct dirent) <= max_size) {
        struct dirent *ent = fs_readdir(f->node, ctx->ent);
        if (!ent) {
            break;
        }

        // !!!: HACK: SET d_reclen BECAUSE IT NORMALLY ISNT SET
        ent->d_reclen = sizeof(struct dirent);

        memcpy(p, ent, sizeof(struct dirent));
        kfree(ent);

        ctx->ent++;
        p += sizeof(struct dirent);
        read += sizeof(struct dirent);
    }  

    return read;
}