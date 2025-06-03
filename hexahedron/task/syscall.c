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
#include <kernel/drivers/net/socket.h>
#include <kernel/fs/vfs.h>
#include <kernel/misc/args.h>
#include <kernel/mem/alloc.h>
#include <kernel/debug.h>
#include <kernel/panic.h>
#include <kernel/gfx/gfx.h>
#include <kernel/gfx/term.h>
#include <kernel/config.h>

#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

/* System call table */
static syscall_func_t syscall_table[] = {
    [SYS_EXIT]          = (syscall_func_t)(uintptr_t)sys_exit,
    [SYS_OPEN]          = (syscall_func_t)(uintptr_t)sys_open,
    [SYS_READ]          = (syscall_func_t)(uintptr_t)sys_read,
    [SYS_WRITE]         = (syscall_func_t)(uintptr_t)sys_write,
    [SYS_CLOSE]         = (syscall_func_t)(uintptr_t)sys_close,
    [SYS_STAT]          = (syscall_func_t)(uintptr_t)sys_stat,
    [SYS_FSTAT]         = (syscall_func_t)(uintptr_t)sys_fstat,
    [SYS_LSTAT]         = (syscall_func_t)(uintptr_t)sys_lstat,
    [SYS_IOCTL]         = (syscall_func_t)(uintptr_t)sys_ioctl,
    [SYS_READDIR]       = (syscall_func_t)(uintptr_t)sys_readdir,
    [SYS_POLL]          = (syscall_func_t)(uintptr_t)sys_poll,
    [SYS_BRK]           = (syscall_func_t)(uintptr_t)sys_brk,
    [SYS_FORK]          = (syscall_func_t)(uintptr_t)sys_fork,
    [SYS_LSEEK]         = (syscall_func_t)(uintptr_t)sys_lseek,
    [SYS_GETTIMEOFDAY]  = (syscall_func_t)(uintptr_t)sys_gettimeofday,
    [SYS_SETTIMEOFDAY]  = (syscall_func_t)(uintptr_t)sys_settimeofday,
    [SYS_USLEEP]        = (syscall_func_t)(uintptr_t)sys_usleep,
    [SYS_EXECVE]        = (syscall_func_t)(uintptr_t)sys_execve,
    [SYS_WAIT]          = (syscall_func_t)(uintptr_t)sys_wait,
    [SYS_GETCWD]        = (syscall_func_t)(uintptr_t)sys_getcwd,
    [SYS_CHDIR]         = (syscall_func_t)(uintptr_t)sys_chdir,
    [SYS_FCHDIR]        = (syscall_func_t)(uintptr_t)sys_fchdir,
    [SYS_UNAME]         = (syscall_func_t)(uintptr_t)sys_uname,
    [SYS_GETPID]        = (syscall_func_t)(uintptr_t)sys_getpid,
    [SYS_TIMES]         = (syscall_func_t)(uintptr_t)0xCAFECAFE,
    [SYS_MMAP]          = (syscall_func_t)(uintptr_t)sys_mmap,
    [SYS_MUNMAP]        = (syscall_func_t)(uintptr_t)sys_munmap,
    [SYS_MSYNC]         = (syscall_func_t)(uintptr_t)0xCAFECAFE,
    [SYS_MPROTECT]      = (syscall_func_t)(uintptr_t)0xCAFECAFE,
    [SYS_DUP2]          = (syscall_func_t)(uintptr_t)sys_dup2,
    [SYS_SIGNAL]        = (syscall_func_t)(uintptr_t)sys_signal,
    [SYS_KILL]          = (syscall_func_t)(uintptr_t)sys_kill,
    [SYS_SOCKET]        = (syscall_func_t)(uintptr_t)sys_socket,
    [SYS_SENDMSG]       = (syscall_func_t)(uintptr_t)sys_sendmsg,
    [SYS_RECVMSG]       = (syscall_func_t)(uintptr_t)sys_recvmsg,
    [SYS_SETSOCKOPT]    = (syscall_func_t)(uintptr_t)sys_setsockopt,
    [SYS_BIND]          = (syscall_func_t)(uintptr_t)sys_bind,
    [SYS_CONNECT]       = (syscall_func_t)(uintptr_t)sys_connect,
    [SYS_LISTEN]        = (syscall_func_t)(uintptr_t)sys_listen,
    [SYS_ACCEPT]        = (syscall_func_t)(uintptr_t)sys_accept,
};


/* Unimplemented system call */
#define SYSCALL_UNIMPLEMENTED(syscall) kernel_panic_extended(UNSUPPORTED_FUNCTION_ERROR, "syscall", "*** The system call \"%s\" is unimplemented\n", syscall)

/* Log method */
#define LOG(status, ...) dprintf_module(status, "TASK:SYSCALL", __VA_ARGS__)


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

    return;
}

/**
 * @brief Exit system call
 */
void sys_exit(int status) {
    LOG(DEBUG, "sys_exit %d\n", status);
    process_exit(NULL, status);
}

/**
 * @brief Open system call
 */
int sys_open(const char *pathname, int flags, mode_t mode) {
    // Validate pointer
    SYSCALL_VALIDATE_PTR(pathname);

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
        fs_open(node, flags);
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

    LOG(DEBUG, "sys_open %s flags %d mode %d\n", pathname, flags, mode);
    return fd->fd_number;
}

/**
 * @brief Read system call
 */
ssize_t sys_read(int fd, void *buffer, size_t count) {
    SYSCALL_VALIDATE_PTR_SIZE(buffer, count);

    if (!FD_VALIDATE(current_cpu->current_process, fd)) {
        return -EBADF;
    }

    fd_t *proc_fd = FD(current_cpu->current_process, fd);
    ssize_t i = fs_read(proc_fd->node, proc_fd->offset, count, (uint8_t*)buffer);
    proc_fd->offset += i;

    // LOG(DEBUG, "sys_read fd %d buffer %p count %d\n", fd, buffer, count);
    return i;
}

/**
 * @brief Write system calll
 */
ssize_t sys_write(int fd, const void *buffer, size_t count) {
    SYSCALL_VALIDATE_PTR_SIZE(buffer, count);

    // stdout?
    if (fd == STDOUT_FILE_DESCRIPTOR || fd == STDERR_FILE_DESCRIPTOR) {
        char *buf = (char*)buffer;
        for (size_t i = 0; i < count; i++) terminal_putchar(buf[i]);
        video_updateScreen();
        return count;
    }
    
    if (!FD_VALIDATE(current_cpu->current_process, fd)) {
        return -EBADF;
    }

    fd_t *proc_fd = FD(current_cpu->current_process, fd);
    ssize_t i = fs_write(proc_fd->node, proc_fd->offset, count, (uint8_t*)buffer);
    proc_fd->offset += i;

    // LOG(DEBUG, "sys_write fd %d buffer %p count %d\n", fd, buffer, count);
    return i;
}

/**
 * @brief Close system call
 */
int sys_close(int fd) {
    if (!FD_VALIDATE(current_cpu->current_process, fd)) {
        return -EBADF;
    }

    LOG(DEBUG, "sys_close fd %d\n", fd);
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
    statbuf->st_blksize = STAT_DEFAULT_BLOCK_SIZE; // TODO: This would prove useful for file I/O
    statbuf->st_blocks = 0; // TODO
    statbuf->st_atime = f->atime;
    statbuf->st_mtime = f->mtime;
    statbuf->st_ctime = f->ctime;
}

/**
 * @brief Stat system call
 */
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

/**
 * @brief fstat system call
 */
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
    }


    // Else, "handle"
    current_cpu->current_process->heap = (uintptr_t)addr;   // Sure.. you can totally have this memory ;)
                                                            // (page fault handler will map this on a critical failure)

    vas_reserve(current_cpu->current_process->vas, current_cpu->current_process->heap, (uintptr_t)addr - current_cpu->current_process->heap);


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

    LOG(DEBUG, "sys_usleep %d\n", usec);
    sleep_untilTime(current_cpu->current_thread, (usec / 10000) / 1000, (usec / 10000) % 1000);
    if (sleep_enter() == WAKEUP_SIGNAL) return -EINTR;

    LOG(DEBUG, "resuming process\n");

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
    if (f->flags != VFS_FILE) { fs_close(f); return -EISDIR; }

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
    char *new_argv[argc+1];
    for (int a = 0; a < argc; a++) {
        new_argv[a] = strdup(argv[a]); // TODO: Leaking memory!!!
    }

    // Reallocate envp if specified
    char *new_envp[envc+1];
    if (envp) {
        for (int e = 0; e < envc; e++) {
            new_envp[e] = strdup(envp[e]); // TODO: Leaking memory!!!
        }
    } 

    // Set null terminators
    new_argv[argc] = NULL;
    new_envp[envc] = NULL;

    process_execute(f, argc, new_argv, new_envp);
    return -ENOEXEC;
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
            LOG(DEBUG, "Hit on file descriptor %d for events %s %s\n", fds[i].fd, (ready & VFS_EVENT_READ) ? "VFS_EVENT_READ" : "", (ready & VFS_EVENT_WRITE) ? "VFS_EVENT_WRITE" : "");
            fds[i].revents = (events & VFS_EVENT_READ && ready & VFS_EVENT_READ) ? POLLIN : 0 | (events & VFS_EVENT_WRITE && ready & VFS_EVENT_WRITE) ? POLLOUT : 0;
            return 1;
        } 
    }

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

    LOG(INFO, "Woken up from a poll due to reason %d\n", wakeup);
    if (wakeup == WAKEUP_SIGNAL) return -EINTR;
    if (wakeup == WAKEUP_TIME) return 0;

    for (size_t i = 0; i < nfds; i++) {
        // Does the file descriptor have available contents right now?
        int events = ((fds[i].events & POLLIN) ? VFS_EVENT_READ : 0) | ((fds[i].events & POLLOUT) ? VFS_EVENT_WRITE : 0);
        int ready = fs_ready(FD(current_cpu->current_process, fds[i].fd)->node, events);

        if (ready & events) {
            LOG(DEBUG, "Hit on file descriptor %d for events %s %s\n", fds[i].fd, (ready & VFS_EVENT_READ) ? "VFS_EVENT_READ" : "", (ready & VFS_EVENT_WRITE) ? "VFS_EVENT_WRITE" : "");
            fds[i].revents = (events & VFS_EVENT_READ && ready & VFS_EVENT_READ) ? POLLIN : 0 | (events & VFS_EVENT_WRITE && ready & VFS_EVENT_WRITE) ? POLLOUT : 0;
            return 1;
        } 
    }

    return 1;   // At least one thread woke us up
}


long sys_uname(struct utsname *buf) {
    SYSCALL_VALIDATE_PTR(buf);

    // Copy!
    snprintf(buf->sysname, LIBPOLYHEDRON_UTSNAME_LENGTH, "Hexahedron");
    snprintf(buf->nodename, LIBPOLYHEDRON_UTSNAME_LENGTH, "N/A"); // lol, should be hostname
    snprintf(buf->release, LIBPOLYHEDRON_UTSNAME_LENGTH, "%d.%d.%d-%s", 
                    __kernel_version_major, 
                    __kernel_version_minor, 
                    __kernel_version_lower,
                    __kernel_build_configuration);

    snprintf(buf->version, LIBPOLYHEDRON_UTSNAME_LENGTH, "%s %s %s",
        __kernel_version_codename,
        __kernel_build_date,
        __kernel_build_time);

    snprintf(buf->machine, LIBPOLYHEDRON_UTSNAME_LENGTH, "%s", __kernel_architecture);

    return 0;
}

pid_t sys_getpid() {
    return current_cpu->current_process->pid;
}

long sys_mmap(sys_mmap_context_t *context) {
    SYSCALL_VALIDATE_PTR(context);
    return (long)process_mmap(context->addr, context->len, context->prot, context->flags, context->filedes, context->off);
}

long sys_munmap(void *addr, size_t len) {
    return process_munmap(addr, len);
}

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

long sys_signal(int signum, sa_handler handler) {
    // Validate range
    if (signum < 0 || signum >= NUMSIGNALS) return -EINVAL;
    if (signum == SIGKILL || signum == SIGSTOP) return -EINVAL; // Trying to set signals that cannot be handled

    // Is the handler special?
    sa_handler old_handler = PROCESS_SIGNAL(current_cpu->current_process, signum).handler;
    if (handler == SIG_IGN) {
        PROCESS_SIGNAL(current_cpu->current_process, signum).handler = SIGNAL_ACTION_IGNORE;
    } else if (handler == SIG_DFL) {
        PROCESS_SIGNAL(current_cpu->current_process, signum).handler = SIGNAL_ACTION_DEFAULT;
    } else {
        // Yes, the kernel will "fail" to catch this if handler is an invalid argument
        // ..but this executes in usermode so it's really us who are laughing
        PROCESS_SIGNAL(current_cpu->current_process, signum).handler = handler;
    }

    PROCESS_SIGNAL(current_cpu->current_process, signum).flags = SA_RESTART;
    return (long)old_handler;
}

long sys_kill(pid_t pid, int sig) {
    // Check signal
    if (sig < 0 || sig >= NUMSIGNALS) return -EINVAL;

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