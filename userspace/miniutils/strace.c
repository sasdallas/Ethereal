/**
 * @file userspace/miniutils/strace.c
 * @brief System call trace utility
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <getopt.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/signal.h>
#include <sys/syscall.h>
#include <string.h>
#include <unistd.h>
#include <ethereal/user.h>
#include <ctype.h>

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
    [SYS_READDIR] = "readdir",
    [SYS_POLL] = "poll",
    [SYS_MKDIR] = "mkdir",
    [SYS_PSELECT] = "pselect",
    [SYS_READLINK] = "readlink",
    [SYS_ACCESS] = "access",
    [SYS_CHMOD] = "chmod",
    [SYS_FCNTL] = "fcntl",
    [SYS_UNLINK] = "unlink",
    [SYS_FTRUNCATE] = "ftruncate",
    [SYS_BRK] = "brk",
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
};

/* Number of system calls */
const int num_syscalls = sizeof(syscall_table) / sizeof(char*);

/* Special printers (for different arguments) */
typedef void (*printer_t)(struct user_regs_struct*);
printer_t special_printers[sizeof(syscall_table) / sizeof(char*)] = {
    NULL,
};

void usage() {
    fprintf(stderr, "Usage: strace [-h] [-v] [PROGRAM]\n");
    fprintf(stderr, "System call tracer utility\n\n");
    fprintf(stderr, " -h, --help        Display this help message\n");
    fprintf(stderr, " -V, --version     Display program version\n");
    exit(EXIT_FAILURE);
}

void version() {
    printf("strace (Ethereal miniutils) 1.00\n");
    printf("Copyright (C) 2025 The Ethereal Development Team\n");
    exit(EXIT_SUCCESS);
}

void print_system_call(struct user_regs_struct *uregs) {
    printf("\033[34m%s()\033[0m\n", syscall_table[uregs->rax]);
}   

int main(int argc, char *argv[]) {
    struct option options[] = {
        { .name = "help", .flag = NULL, .has_arg = no_argument, .val = 'h' },
        { .name = "version", .flag = NULL, .has_arg = no_argument, .val = 'V' },
        { .name = NULL, .has_arg = no_argument, .flag = NULL, .val = 0 },
    };

    int index;
    int ch;

    while ((ch = getopt_long(argc, argv, "hv", options, &index)) != -1) {
        if (!ch && options[index].flag == NULL) ch = options[index].val;

        switch (ch) {
            case 'V':
                version();
                break;

            case 'h':
            default:
                usage();
                break;
        }
    } 

    if (argc-optind < 1) {
        usage();
    }

    printf("Starting trace of program: \"%s\"\n", argv[optind]);

    pid_t child = fork();
    if (child == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (child == 0) {
        if (ptrace(PTRACE_TRACEME, 0, NULL, NULL) == -1) {
            perror("ptrace");
            exit(EXIT_FAILURE);
        }

        execvp(argv[optind], &argv[optind]);
        perror("execvp");
        exit(EXIT_FAILURE);
    } else {
        int status;
        while (1) {
            if (waitpid(child, &status, WSTOPPED) == -1) {
                perror("waitpid");
                exit(EXIT_FAILURE);
            }

            if (WIFEXITED(status)) {
                printf("Child process exited with status %d\n", WEXITSTATUS(status));
                break;
            }

            if (WIFSTOPPED(status)) {
                if (WSTOPSIG(status) == SIGSTOP) {
                    printf("\033[38;5;4mBeginning trace of child process\033[0m\n");
                    ptrace(PTRACE_SYSCALL, child, NULL, NULL);
                    continue; // Initial trap after PTRACE_TRACEME
                }

                if (WSTOPSIG(status) != SIGTRAP) {
                    printf("\033[0;34mWARNING:\033[0m Unrecognized stop signal %s\n", strsignal(WSTOPSIG(status)));
                    ptrace(PTRACE_SYSCALL, child, NULL, NULL);
                    continue;
                }

                // We are stopped at a SIGTRAP, read registers
                // TODO: Maybe make ptrace API more compatible... This assumes Ethereal API usage

                struct user_regs_struct regs;
                memset(&regs, 0xFF, sizeof(struct user_regs_struct));
                ptrace(PTRACE_GETREGS, child, 0, &regs);

                print_system_call(&regs);
                ptrace(PTRACE_SYSCALL, child, NULL, NULL);
            }
        }
    }

    return 0;

}