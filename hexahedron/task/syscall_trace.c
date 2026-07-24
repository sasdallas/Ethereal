/**
 * @file hexahedron/task/syscall_trace.c
 * @brief System call tracer 9000
 * 
 * This was inspired by the Astral Operating System's syscall tracer,
 * of which he suggested I add one too.
 * 
 * @copyright
 * This file is part of the Hexahedron kernel which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#include <sys/syscall.h>
#include <sys/syscall_nums.h>
#include <kernel/debug.h>
#include <kernel/arch/arch.h>
#include <kernel/task/process.h>
#include <stdio.h>
#include <errno.h>

char *syscall_table[] = {
    [SYS_EXIT] = "exit",
    NULL,
    [SYS_OPEN] = "open",
    [SYS_READ] = "read",
    [SYS_WRITE] = "write",
    [SYS_CLOSE] = "close",
    [SYS_STAT] = "stat",
    [SYS_FSTAT] = "fstat",
    [SYS_LSTAT] = "lstat",
    [SYS_IOCTL] = "ioctl",
    [SYS_POLL] = "poll",
    [SYS_MKDIR] = "mkdir",
    [SYS_PSELECT] = "pselect",
    [SYS_READLINK] = "readlink",
    [SYS_ACCESS] = "access",
    [SYS_CHMOD] = "chmod",
    [SYS_FCNTL] = "fcntl",
    [SYS_UNLINKAT] = "unlink",
    [SYS_FTRUNCATE] = "ftruncate",
    [SYS_FORK] = "fork",
    [SYS_LSEEK] = "lseek",
    [SYS_GETTIMEOFDAY] = "gettimeofday",
    [SYS_SETTIMEOFDAY] = "settimeofday",
    [SYS_USLEEP] = "usleep",
    [SYS_EXECVE] = "execve",
    [SYS_WAIT] = "wait",
    [SYS_GETCWD] = "getcwd",
    [SYS_CHDIR] = "chdir",
    [SYS_FCHDIR] = "fchdir",
    [SYS_UNAME] = "uname",
    [SYS_GETPID] = "getpid",
    [SYS_TIMES] = "times",
    [SYS_MMAP] = "mmap",
    [SYS_MPROTECT] = "mprotect",
    [SYS_MUNMAP] = "munmap",
    [SYS_MSYNC] = "msync",
    [SYS_DUP2] = "dup2",
    [SYS_SIGNAL] = "signal",
    [SYS_SIGACTION] = "sigaction",
    [SYS_SIGPENDING] = "sigpending",
    [SYS_SIGPROCMASK] = "sigprocmask",
    [SYS_SIGSUSPEND] = "sigsuspend",
    [SYS_SIGWAIT] = "sigwait",
    [SYS_KILL] = "kill",
    [SYS_SOCKET] = "socket",
    [SYS_BIND] = "bind",
    [SYS_ACCEPT] = "accept",
    [SYS_LISTEN] = "listen",
    [SYS_CONNECT] = "connect",
    [SYS_GETSOCKOPT] = "getsockopt",
    [SYS_SETSOCKOPT] = "setsockopt",
    [SYS_SENDMSG] = "sendmsg",
    [SYS_RECVMSG] = "recvmsg",
    [SYS_SHUTDOWN] = "shutdown",
    [SYS_GETSOCKNAME] = "getsockname",
    [SYS_GETPEERNAME] = "getpeername",
    [SYS_SOCKETPAIR] = "socketpair",
    [SYS_MOUNT] = "mount",
    [SYS_UMOUNT] = "umount",
    [SYS_PIPE] = "pipe",
    [SYS_SHARED_NEW] = "shared",
    [SYS_SHARED_KEY] = "shared",
    [SYS_SHARED_OPEN] = "shared",
    [SYS_CREATE_THREAD] = "create",
    [SYS_GETTID] = "gettid",
    [SYS_SETTLS] = "settls",
    [SYS_EXIT_THREAD] = "exit",
    [SYS_JOIN_THREAD] = "join",
    [SYS_KILL_THREAD] = "kill",
    [SYS_EPOLL_CREATE] = "epoll",
    [SYS_EPOLL_CTL] = "epoll",
    [SYS_EPOLL_PWAIT] = "epoll",
    [SYS_OPENPTY] = "openpty",
    [SYS_GETUID] = "getuid",
    [SYS_SETUID] = "setuid",
    [SYS_GETGID] = "getgid",
    [SYS_SETGID] = "setgid",
    [SYS_GETPPID] = "getppid",
    [SYS_GETPGID] = "getpgid",
    [SYS_SETPGID] = "setpgid",
    [SYS_GETSID] = "getsid",
    [SYS_SETSID] = "setsid",
    [SYS_GETEUID] = "geteuid",
    [SYS_SETEUID] = "seteuid",
    [SYS_GETEGID] = "getegid",
    [SYS_SETEGID] = "setegid",
    [SYS_GETHOSTNAME] = "gethostname",
    [SYS_SETHOSTNAME] = "sethostname",
    [SYS_YIELD] = "yield",
    [SYS_LOAD_DRIVER] = "load_driver",
    [SYS_UNLOAD_DRIVER] = "unload_driver",
    [SYS_GET_DRIVER] = "get_driver",
    [SYS_SETITIMER] = "setitimer",
    [SYS_PTRACE] = "ptrace",
    [SYS_REBOOT] = "reboot",
    [SYS_READ_ENTRIES] = "read_entries",
    [SYS_FUTEX_WAIT] = "futex_wait",
    [SYS_FUTEX_WAKE] = "futex_wake",
    [SYS_OPENAT] = "openat",
    [SYS_RENAMEAT] = "renameat",
    [SYS_LINKAT] = "linkat",
    [SYS_SYMLINKAT] = "symlinkat",
    [SYS_FCHMODAT] = "fchmodat",
    [SYS_MKNODAT] = "mknodat",
    [SYS_FLOCK] = "flock",
    [SYS_UMASK] = "umask",
    [SYS_CLOCK_GETTIME] = "clock_gettime",
    [SYS_CLOCK_SETTIME] = "clock_settime",
    [SYS_FSYNC] = "fsync",
    [SYS_PREAD] = "pread",
    [SYS_PWRITE] = "pwrite",
    [SYS_GETRLIMIT] = "getrlimit",
    [SYS_SETRLIMIT] = "setrlimit",
    [SYS_PAUSE] = "pause",
    [SYS_FCHOWNAT] = "fchownat",
    [SYS_FACCESSAT] = "faccessat",
    [SYS_SYNC] = "sync",
};

char *syscall_formats[] = {
    [SYS_EXIT] = "status %d",
    [SYS_OPEN] = "pathname %s flags %d mode %d",
    [SYS_READ] = "fd %d buf %p count %zu",
    [SYS_WRITE] = "fd %d buf %p count %zu",
    [SYS_CLOSE] = "fd %d",
    [SYS_STAT] = "pathname %s statbuf %p",
    [SYS_FSTAT] = "fd %d statbuf %p",
    [SYS_LSTAT] = "pathname %s statbuf %p",
    [SYS_IOCTL] = "fd %d request %lu argp %p",
    [SYS_POLL] = "fds %p nfds %d timeout %d",
    [SYS_MKDIR] = "pathname %s mode %d",
    [SYS_PSELECT] = "nfds %d readfds %p writefds %p exceptfds %p timeout %p sigmask %p",
    [SYS_READLINK] = "pathname %s buf %s bufsiz %zu",
    [SYS_ACCESS] = "pathname %s mode %d",
    [SYS_CHMOD] = "pathname %s mode %d",
    [SYS_FCNTL] = "fd %d cmd %d arg %ld",
    [SYS_UNLINKAT] = "dfd %d pathname %s flag %d",
    [SYS_FTRUNCATE] = "fd %d length %ld",
    [SYS_FORK] = "",
    [SYS_LSEEK] = "fd %d offset %ld whence %d",
    [SYS_GETTIMEOFDAY] = "tv %p tz %p",
    [SYS_SETTIMEOFDAY] = "tv %p tz %p",
    [SYS_USLEEP] = "usec %u",
    [SYS_EXECVE] = "pathname %s argv %p envp %p",
    [SYS_WAIT] = "wstatus %p",
    [SYS_GETCWD] = "buf %s size %zu",
    [SYS_CHDIR] = "path %s",
    [SYS_FCHDIR] = "fd %d",
    [SYS_UNAME] = "buf %p",
    [SYS_GETPID] = "",
    [SYS_TIMES] = "buf %p",
    [SYS_MMAP] = "addr %p length %zu prot %d flags %d fd %d offset %ld",
    [SYS_MPROTECT] = "addr %p len %zu prot %d",
    [SYS_MUNMAP] = "addr %p length %zu",
    [SYS_MSYNC] = "addr %p length %zu flags %d",
    [SYS_DUP2] = "oldfd %d newfd %d",
    [SYS_SIGNAL] = "signum %d handler %p",
    [SYS_SIGACTION] = "signum %d act %p oldact %p",
    [SYS_SIGPENDING] = "set %p",
    [SYS_SIGPROCMASK] = "how %d set %p oldset %p",
    [SYS_SIGSUSPEND] = "mask %p",
    [SYS_SIGWAIT] = "set %p sig %p",
    [SYS_KILL] = "pid %d sig %d",
    [SYS_SOCKET] = "domain %d type %d protocol %d",
    [SYS_BIND] = "sockfd %d addr %p addrlen %u",
    [SYS_ACCEPT] = "sockfd %d addr %p addrlen %p",
    [SYS_LISTEN] = "sockfd %d backlog %d",
    [SYS_CONNECT] = "sockfd %d addr %p addrlen %u",
    [SYS_GETSOCKOPT] = "sockfd %d level %d optname %d optval %p optlen %p",
    [SYS_SETSOCKOPT] = "sockfd %d level %d optname %d optval %p optlen %u",
    [SYS_SENDMSG] = "sockfd %d msg %p flags %d",
    [SYS_RECVMSG] = "sockfd %d msg %p flags %d",
    [SYS_SHUTDOWN] = "sockfd %d how %d",
    [SYS_GETSOCKNAME] = "sockfd %d addr %p addrlen %p",
    [SYS_GETPEERNAME] = "sockfd %d addr %p addrlen %p",
    [SYS_SOCKETPAIR] = "domain %d type %d protocol %d sv %p",
    [SYS_MOUNT] = "source %s target %s filesystemtype %s mountflags %lu data %p",
    [SYS_UMOUNT] = "target %s",
    [SYS_PIPE] = "pipefd %p",
    [SYS_SHARED_NEW] = "size %zu flags 0x%x",
    [SYS_SHARED_KEY] = "fd %d",
    [SYS_SHARED_OPEN] = "key %d",
    [SYS_CREATE_THREAD] = "stack %p tls %p entry %p arg %p",
    [SYS_GETTID] = "",
    [SYS_SETTLS] = "tls %p",
    [SYS_EXIT_THREAD] = "???",
    [SYS_JOIN_THREAD] = "tid %d retval %p",
    [SYS_KILL_THREAD] = "tid %d sig %d",
    [SYS_EPOLL_CREATE] = "size %d",
    [SYS_EPOLL_CTL] = "epfd %d op %d fd %d event %p",
    [SYS_EPOLL_PWAIT] = "epfd %d events %p maxevents %d timeout %d sigmask %p",
    [SYS_OPENPTY] = "amaster %p aslave %p name %s termp %p winp %p",
    [SYS_GETUID] = "",
    [SYS_SETUID] = "uid %d",
    [SYS_GETGID] = "",
    [SYS_SETGID] = "gid %d",
    [SYS_GETPPID] = "",
    [SYS_GETPGID] = "pid %d",
    [SYS_SETPGID] = "pid %d pgid %d",
    [SYS_GETSID] = "pid %d",
    [SYS_SETSID] = "",
    [SYS_GETEUID] = "",
    [SYS_SETEUID] = "euid %d",
    [SYS_GETEGID] = "",
    [SYS_SETEGID] = "egid %d",
    [SYS_GETHOSTNAME] = "name %p len %zu",
    [SYS_SETHOSTNAME] = "name %s len %zu",
    [SYS_YIELD] = "",
    [SYS_LOAD_DRIVER] = "filename %s priority %d argv %p",
    [SYS_UNLOAD_DRIVER] = "id %d",
    [SYS_GET_DRIVER] = "id %d driver %p",
    [SYS_SETITIMER] = "which %d new_value %p old_value %p",
    [SYS_PTRACE] = "request %d pid %d addr %p data %p",
    [SYS_REBOOT] = "magic %d magic2 %d cmd %d arg %p",
    [SYS_READ_ENTRIES] = "fd %d buffer %p max_size %zu",
    [SYS_FUTEX_WAIT] = "uaddr %p val %d timeout %p",
    [SYS_FUTEX_WAKE] = "uaddr %p val %d",
    [SYS_OPENAT] = "dirfd %d pathname %s flags %d mode %d",
    [SYS_RENAMEAT] = "olddirfd %d oldpath %s newdirfd %d newpath %s",
    [SYS_LINKAT] = "olddirfd %d oldpath %s newdirfd %d newpath %s flags %d",
    [SYS_SYMLINKAT] = "target %s newdirfd %d linkpath %s",
    [SYS_FCHMODAT] = "dirfd %d pathname %s mode %d flags %d",
    [SYS_MKNODAT] = "dirfd %d pathname %s mode %d dev %lu",
    [SYS_FLOCK] = "fd %d operation %d",
    [SYS_UMASK] = "mask %d",
    [SYS_CLOCK_GETTIME] = "clk_id %d tp %p",
    [SYS_CLOCK_SETTIME] = "clk_id %d tp %p",
    [SYS_FSYNC] = "fd %d",
    [SYS_PREAD] = "fd %d buf %p count %zu offset %ld",
    [SYS_PWRITE] = "fd %d buf %p count %zu offset %ld",
    [SYS_GETRLIMIT] = "resource %d rlim %p",
    [SYS_SETRLIMIT] = "resource %d rlim %p",
    [SYS_PAUSE] = "",
    [SYS_FCHOWNAT] = "dirfd %d pathname %s owner %d group %d flags %d",
    [SYS_FACCESSAT] = "dirfd %d pathname %s mode %d flags %d",
    [SYS_SYNC] = "",
};

spinlock_t lk = SPINLOCK_INITIALIZER;

static inline void write_e9(char *str) {
    while (*str) {
        outportb(0xe9, *str++);
    }
}


char *strerror(int e) {
	char *s;
	switch(e) {
	case EAGAIN: s = "Operation would block (EAGAIN)"; break;
	case EACCES: s = "Access denied (EACCESS)"; break;
	case EBADF:  s = "Bad file descriptor (EBADF)"; break;
	case EEXIST: s = "File exists already (EEXIST)"; break;
	case EFAULT: s = "Access violation (EFAULT)"; break;
	case EINTR:  s = "Operation interrupted (EINTR)"; break;
	case EINVAL: s = "Invalid argument (EINVAL)"; break;
	case EIO:    s = "I/O error (EIO)"; break;
	case EISDIR: s = "Resource is directory (EISDIR)"; break;
	case ENOENT: s = "No such file or directory (ENOENT)"; break;
	case ENOMEM: s = "Out of memory (ENOMEM)"; break;
	case ENOTDIR: s = "Expected directory instead of file (ENOTDIR)"; break;
	case ENOSYS: s = "Operation not implemented (ENOSYS)"; break;
	case EPERM:  s = "Operation not permitted (EPERM)"; break;
	case EPIPE:  s = "Broken pipe (EPIPE)"; break;
	case ESPIPE: s = "Seek not possible (ESPIPE)"; break;
	case ENXIO: s = "No such device or address (ENXIO)"; break;
	case ENOEXEC: s = "Exec format error (ENOEXEC)"; break;
	case ENOSPC: s = "No space left on device (ENOSPC)"; break;
	case ENOTSOCK: s = "Socket operation on non-socket (ENOTSOCK)"; break;
	case ENOTCONN: s = "Transport endpoint is not connected (ENOTCONN)"; break;
	case EDOM: s = "Numerical argument out of domain (EDOM)"; break;
	case EILSEQ: s = "Invalid or incomplete multibyte or wide character (EILSEQ)"; break;
	case ERANGE: s = "Numerical result out of range (ERANGE)"; break;
	case E2BIG: s = "Argument list too long (E2BIG)"; break;
	case EADDRINUSE: s = "Address already in use (EADDRINUSE)"; break;
	case EADDRNOTAVAIL: s = "Cannot assign requested address (EADDRNOTAVAIL)"; break;
	case EAFNOSUPPORT: s = "Address family not supported by protocol (EAFNOSUPPORT)"; break;
	case EALREADY: s = "Operation already in progress (EALREADY)"; break;
	case EBADMSG: s = "Bad message (EBADMSG)"; break;
	case EBUSY: s = "Device or resource busy (EBUSY)"; break;
	case ECANCELED: s = "Operation canceled (ECANCELED)"; break;
	case ECHILD: s = "No child processes (ECHILD)"; break;
	case ECONNABORTED: s = "Software caused connection abort (ECONNABORTED)"; break;
	case ECONNREFUSED: s = "Connection refused (ECONNREFUSED)"; break;
	case ECONNRESET: s = "Connection reset by peer (ECONNRESET)"; break;
	case EDEADLK: s = "Resource deadlock avoided (EDEADLK)"; break;
	case EDESTADDRREQ: s = "Destination address required (EDESTADDRREQ)"; break;
	case EDQUOT: s = "Disk quota exceeded (EDQUOT)"; break;
	case EFBIG: s = "File too large (EFBIG)"; break;
	case EHOSTUNREACH: s = "No route to host (EHOSTUNREACH)"; break;
	case EIDRM: s = "Identifier removed (EIDRM)"; break;
	case EINPROGRESS: s = "Operation now in progress (EINPROGRESS)"; break;
	case EISCONN: s = "Transport endpoint is already connected (EISCONN)"; break;
	case ELOOP: s = "Too many levels of symbolic links (ELOOP)"; break;
	case EMFILE: s = "Too many open files (EMFILE)"; break;
	case EMLINK: s = "Too many links (EMLINK)"; break;
	case EMSGSIZE: s = "Message too long (EMSGSIZE)"; break;
	case EMULTIHOP: s = "Multihop attempted (EMULTIHOP)"; break;
	case ENAMETOOLONG: s = "File name too long (ENAMETOOLONG)"; break;
	case ENETDOWN: s = "Network is down (ENETDOWN)"; break;
	case ENETRESET: s = "Network dropped connection on reset (ENETRESET)"; break;
	case ENETUNREACH: s = "Network is unreachable (ENETUNREACH)"; break;
	case ENFILE: s = "Too many open files in system (ENFILE)"; break;
	case ENOBUFS: s = "No buffer space available (ENOBUFS)"; break;
	case ENODEV: s = "No such device (ENODEV)"; break;
	case ENOLCK: s = "No locks available (ENOLCK)"; break;
	case ENOLINK: s = "Link has been severed (ENOLINK)"; break;
	case ENOMSG: s = "No message of desired type (ENOMSG)"; break;
	case ENOPROTOOPT: s = "Protocol not available (ENOPROTOOPT)"; break;
	case ENOTEMPTY: s = "Directory not empty (ENOTEMPTY)"; break;
	case ENOTRECOVERABLE: s = "Sate not recoverable (ENOTRECOVERABLE)"; break;
	case ENOTSUP: s = "Operation not supported (ENOTSUP)"; break;
	case ENOTTY: s = "Inappropriate ioctl for device (ENOTTY)"; break;
	case EOVERFLOW: s = "Value too large for defined datatype (EOVERFLOW)"; break;
#if EOPNOTSUPP != ENOTSUP
	/* these are aliases on the mlibc abi */
	case EOPNOTSUPP: s = "Operation not supported (EOPNOTSUP)"; break;
#endif
	case EOWNERDEAD: s = "Owner died (EOWNERDEAD)"; break;
	case EPROTO: s = "Protocol error (EPROTO)"; break;
	case EPROTONOSUPPORT: s = "Protocol not supported (EPROTONOSUPPORT)"; break;
	case EPROTOTYPE: s = "Protocol wrong type for socket (EPROTOTYPE)"; break;
	case EROFS: s = "Read-only file system (EROFS)"; break;
	case ESRCH: s = "No such process (ESRCH)"; break;
	case ESTALE: s = "Stale file handle (ESTALE)"; break;
	case ETIMEDOUT: s = "Connection timed out (ETIMEDOUT)"; break;
	case ETXTBSY: s = "Text file busy (ETXTBSY)"; break;
	case EXDEV: s = "Invalid cross-device link (EXDEV)"; break;
	case ENODATA: s = "No data available (ENODATA)"; break;
	case ETIME: s = "Timer expired (ETIME)"; break;
	case ENOKEY: s = "Required key not available (ENOKEY)"; break;
	case ESHUTDOWN: s = "Cannot send after transport endpoint shutdown (ESHUTDOWN)"; break;
	case EHOSTDOWN: s = "Host is down (EHOSTDOWN)"; break;
	case EBADFD: s = "File descriptor in bad state (EBADFD)"; break;
	case ENOMEDIUM: s = "No medium found (ENOMEDIUM)"; break;
	case ENOTBLK: s = "Block device required (ENOTBLK)"; break;
	case ENONET: s = "Machine is not on the network (ENONET)"; break;
	case EPFNOSUPPORT: s = "Protocol family not supported (EPFNOSUPPORT)"; break;
	case ESOCKTNOSUPPORT: s = "Socket type not supported (ESOCKTNOSUPPORT)"; break;
	case ESTRPIPE: s = "Streams pipe error (ESTRPIPE)"; break;
	case EREMOTEIO: s = "Remote I/O error (EREMOTEIO)"; break;
	case ERFKILL: s = "Operation not possible due to RF-kill (ERFKILL)"; break;
	case EBADR: s = "Invalid request descriptor (EBADR)"; break;
	case EUNATCH: s = "Protocol driver not attached (EUNATCH)"; break;
	case EMEDIUMTYPE: s = "Wrong medium type (EMEDIUMTYPE)"; break;
	case EREMOTE: s = "Object is remote (EREMOTE)"; break;
	case EKEYREJECTED: s = "Key was rejected by service (EKEYREJECTED)"; break;
	case EUCLEAN: s = "Structure needs cleaning (EUCLEAN)"; break;
	case EBADSLT: s = "Invalid slot (EBADSLT)"; break;
	case ENOANO: s = "No anode (ENOANO)"; break;
	case ENOCSI: s = "No CSI structure available (ENOCSI)"; break;
	case ENOSTR: s = "Device not a stream (ENOSTR)"; break;
	case ETOOMANYREFS: s = "Too many references: cannot splice (ETOOMANYREFS)"; break;
	case ENOPKG: s = "Package not installed (ENOPKG)"; break;
	case EKEYREVOKED: s = "Key has been revoked (EKEYREVOKED)"; break;
	case EXFULL: s = "Exchange full (EXFULL)"; break;
	case ELNRNG: s = "Link number out of range (ELNRNG)"; break;
	case ENOTUNIQ: s = "Name not unique on network (ENOTUNIQ)"; break;
	case ERESTART: s = "Interrupted system call should be restarted (ERESTART)"; break;
	case EUSERS: s = "Too many users (EUSERS)"; break;

#ifdef EIEIO
	case EIEIO: s = "Computer bought the farm; OS internal error (EIEIO)"; break;
#endif

	default:
		s = "Unknown error code (?)";
	}
	return (s);
}

void syscall_trace_enter(syscall_t *s) {
    thread_t *thr = current_cpu->current_thread;
    process_t *p = current_cpu->current_process;

    char *fmt =  syscall_formats[s->syscall_number];
    char *name = syscall_table[s->syscall_number];


    char sysbuf[512];
    
    if (s->syscall_number == SYS_MMAP) {
        sys_mmap_context_t *c = (typeof(c))s->parameters[0];
        snprintf(sysbuf, 512, fmt, c->addr, c->len, c->prot, c->flags, c->filedes, c->off);
    } else if (s->syscall_number == SYS_SETSOCKOPT) {
        sys_setopt_context_t *c = (typeof(c))s->parameters[0];
        snprintf(sysbuf, 512, fmt, c->socket, c->level, c->option_name, c->option_value, c->option_len);
    } else {
        snprintf(sysbuf, 512, fmt, s->parameters[0], s->parameters[1], s->parameters[2], s->parameters[3], s->parameters[4]);
    }

    char buffer[1024];
    snprintf(buffer, 1024, "syscall: process %s:%d thread %d: %s: %s\r\n", p->name, p->pid, thr->tid, name, sysbuf);

    spinlock_acquire(&lk);
    write_e9(buffer);
    spinlock_release(&lk);
}

void syscall_trace_exit(syscall_t *s) {
    thread_t *thr = current_cpu->current_thread;
    process_t *p = current_cpu->current_process;

    char buffer[1024];
    char *name = syscall_table[s->syscall_number];

    if ((int)s->return_value < 0) {
        snprintf(buffer, 1024, "return: process %s:%d thread %d: %s: %s (0x%llx)\r\n", p->name, p->pid, thr->tid, name, strerror(-s->return_value), s->return_value);
    } else {
        snprintf(buffer, 1024, "return: process %s:%d thread %d: %s: %lld\r\n", p->name, p->pid, thr->tid, name, s->return_value);
    }


    spinlock_acquire(&lk);
    write_e9(buffer);
    spinlock_release(&lk);
}
