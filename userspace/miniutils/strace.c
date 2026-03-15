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
#include <sys/socket.h>

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
    [SYS_UNLINKAT] = "unlink",
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
    [SYS_FUTEX_WAIT] = "futex_wait",
    [SYS_FUTEX_WAKE] = "futex_wake",
    [SYS_OPENAT] = "openat",
};

/* Printer prototypes */
void close_print(struct user_regs_struct *uregs);
void read_print(struct user_regs_struct *uregs);
void write_print(struct user_regs_struct *uregs);
void lseek_print(struct user_regs_struct *uregs);
void socket_print(struct user_regs_struct *uregs);
void ioctl_print(struct user_regs_struct *uregs);
void open_print(struct user_regs_struct *uregs);
void lstat_print(struct user_regs_struct *uregs);
void mmap_print(struct user_regs_struct *uregs);

/* Number of system calls */
const int num_syscalls = sizeof(syscall_table) / sizeof(char*);

/* Special printers (for different arguments) */
typedef void (*printer_t)(struct user_regs_struct*);

/* Child */
pid_t child;

/* List table that is converted to quick access list */
struct printer_list_entry {
    int system_call;
    printer_t p;
    int use_hex;
} special_printers[] = {
    { .system_call = SYS_READ, .p = read_print, .use_hex = 0 },
    { .system_call = SYS_WRITE, .p = write_print, .use_hex = 0 },
    { .system_call = SYS_CLOSE, .p = close_print, .use_hex = 0 },
    { .system_call = SYS_LSEEK, .p = lseek_print, .use_hex = 0 },
    { .system_call = SYS_IOCTL, .p = ioctl_print, .use_hex = 0 },
    { .system_call = SYS_SOCKET, .p = socket_print, .use_hex = 0 },
    { .system_call = SYS_OPEN, .p = open_print, .use_hex = 0 },
    { .system_call = SYS_LSTAT, .p = lstat_print, .use_hex = 0 },
    { .system_call = SYS_MMAP, .p = mmap_print, .use_hex = 1 }
};

/* Printer lookup table */
struct printer_list_entry **printer_lookup_table;


/* This system call */
int last_syscall = 0;

/* Helper macros */
#define CASE_PRINT(c) case (c): printf(#c); break;

#include <stdarg.h>

int syscall_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    int ret = 0;
    const char *p = fmt;
    char buf[1024];
    int buf_idx = 0;
    int syscall_named = 0;

    buf_idx += snprintf(buf + buf_idx, sizeof(buf) - buf_idx, "\033[34m");

    while (*p && buf_idx < (int)(sizeof(buf) - 1)) {
        if (p[0] == '%' && p[1] == 's') {
            int n = snprintf(buf + buf_idx, sizeof(buf) - buf_idx, "\033[32m\"%%s\"\033[0m");
            buf_idx += n;
            p += 2;
        } else if (p[0] == '%' && p[1] == 'S') {
            // standard
            int n = snprintf(buf + buf_idx, sizeof(buf) - buf_idx, "%%s");
            buf_idx += n;
            p += 2;
        } else if (p[0] == '%' && p[1] == 'd') {
            int n = snprintf(buf + buf_idx, sizeof(buf) - buf_idx, "\033[36m%%d\033[0m");
            buf_idx += n;
            p += 2;
        } else if (p[0] == '%' && p[1] == 'u') {
            int n = snprintf(buf + buf_idx, sizeof(buf) - buf_idx, "\033[36m%%u\033[0m");
            buf_idx += n;
            p += 2;
        } else if (p[0] == '%' && p[1] == 'p') {
            int n = snprintf(buf + buf_idx, sizeof(buf) - buf_idx, "\033[36m%%p\033[0m");
            buf_idx += n;
            p += 2;
        } else if (p[0] == '(' && !syscall_named) {
            buf_idx += snprintf(buf + buf_idx, sizeof(buf) - buf_idx, "\033[0m");
            syscall_named = 1;
            buf[buf_idx++] = *p++;
        } else {
            buf[buf_idx++] = *p++;
        }
    }
    buf[buf_idx] = '\0';

    ret = vprintf(buf, args);
    va_end(args);
    return ret;
}


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

void read_string(char *buf, size_t max_len, void *addr) {
    size_t i = 0;
    long word;
    unsigned char *p = (unsigned char *)buf;

    while (i < max_len - 1) {
        errno = 0;
        word = ptrace(PTRACE_PEEKDATA, child, (void *)((uintptr_t)addr + i), NULL);
        if (errno != 0) {
            break;
        }
        for (size_t j = 0; j < sizeof(long) && i < max_len - 1; j++, i++) {
            p[i] = ((word >> (8 * j)) & 0xff);
            if (p[i] == '\0') {
                buf[i] = '\0';
                return;
            }
        }
    }
    buf[max_len - 1] = '\0';
}

void print_system_call(struct user_regs_struct *uregs) {
    if (uregs->rax >= num_syscalls) {
        printf("???");
        return;
    }

    if (printer_lookup_table[uregs->rax] && printer_lookup_table[uregs->rax]->p) {
        printer_lookup_table[uregs->rax]->p(uregs);
    } else {
        printf("\033[34m%s\033[0m()", syscall_table[uregs->rax]);
    }
}  

void print_return_value(long long value) {
    int use_hex = 0;
    if (printer_lookup_table[last_syscall]) {
        use_hex = printer_lookup_table[last_syscall]->use_hex;

    }
    
    if ((int)value < 0) {
        printf(" = \033[31m%s\n", strerror(-value));
    } else {
        if (use_hex) {
            printf(" = \033[32m%p\n", value);
        } else {
            printf(" = \033[32m%lld\n", value);
        }
    }
}


/* Fast lookup printer table */
void create_printer_table() {
    printer_lookup_table = malloc(num_syscalls * sizeof(struct printer_list_entry*));
    memset(printer_lookup_table, 0, num_syscalls * sizeof(struct printer_list_entry*));
    for (unsigned i = 0; i < sizeof(special_printers) / sizeof(struct printer_list_entry); i++) {
        struct printer_list_entry *e = malloc(sizeof(struct printer_list_entry));
        memcpy(e, &special_printers[i], sizeof(struct printer_list_entry));
        printer_lookup_table[special_printers[i].system_call] = e;
    }
}

int main(int argc, char *argv[]) {
    struct option options[] = {
        { .name = "help", .flag = NULL, .has_arg = no_argument, .val = 'h' },
        { .name = "version", .flag = NULL, .has_arg = no_argument, .val = 'V' },
        { .name = NULL, .has_arg = no_argument, .flag = NULL, .val = 0 },
    };

    int index;
    int ch;

    opterr = 0;
    while ((ch = getopt_long(argc, argv, "hV", options, &index)) != -1) {
        if (ch == '?') break;
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

    // Create printer table
    create_printer_table();

    printf("Starting trace of program: \"%s\"\n", argv[optind]);

    child = fork();
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
        int in_syscall = 0;

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
                    printf("\033[38;5;4mBeginning trace of child process\n");
                    ptrace(PTRACE_SYSCALL, child, NULL, NULL);
                    continue; // Initial trap after PTRACE_TRACEME
                }

                if (WSTOPSIG(status) != SIGTRAP) {
                    printf("\033[0;34mWARNING: Unrecognized stop signal %s\n", strsignal(WSTOPSIG(status)));
                    ptrace(PTRACE_SYSCALL, child, NULL, NULL);
                    continue;
                }

                // Get registers
                struct user_regs_struct regs;
                memset(&regs, 0, sizeof(regs));
                if (ptrace(PTRACE_GETREGS, child, 0, &regs) == -1) {
                    perror("ptrace(GETREGS)");
                    exit(EXIT_FAILURE);
                }

                if (!in_syscall) {
                    // System call entry
                    print_system_call(&regs);
                    in_syscall = 1;
                    last_syscall = regs.rax;
                    fflush(stdout);
                } else {
                    // Exit of a system call
                    long long ret = (long long)regs.rax;
                    if (ret >= 0 && last_syscall == SYS_EXECVE) {
                        // !!!: ETHEREAL BUG: SYS_EXECVE doesn't return a value or trigger a ptrace on success
                        print_return_value(0);
                        print_system_call(&regs);
                        fflush(stdout);
                        last_syscall = regs.rax;
                        in_syscall = 1;
                    } else {
                        print_return_value(ret);
                        in_syscall = 0;
                    }
                }

                ptrace(PTRACE_SYSCALL, child, NULL, NULL);
            }
        }
    }

    return 0;

}

/**** PRINTERS ****/

void close_print(struct user_regs_struct *uregs) { syscall_printf("close(%d)", uregs->rdi); }
void lseek_print(struct user_regs_struct *uregs) {
    syscall_printf("lseek(%d, %u, \033[33m", uregs->rdi, uregs->rsi);

    switch (uregs->rdx) {
        case SEEK_SET:
            printf("SEEK_SET"); break;
        case SEEK_CUR:
            printf("SEEK_CUR"); break;
        case SEEK_END:
            printf("SEEK_END"); break;
        default:
            printf("???"); break;
    }
    printf("\033[0m)");
}


void read_print(struct user_regs_struct *uregs) { syscall_printf("read(%d, %p, %d)", uregs->rdi, uregs->rsi, uregs->r10); }
void write_print(struct user_regs_struct *uregs) { char buf[128]; read_string(buf, 128, (void*)uregs->rsi); syscall_printf("write(%d, %s, %d)", uregs->rdi, buf, uregs->r10); }
void socket_print(struct user_regs_struct *uregs) {
    syscall_printf("socket(");

    switch (uregs->rdi) {
        CASE_PRINT(AF_UNIX);
        CASE_PRINT(AF_INET);
        CASE_PRINT(AF_AX25);
        CASE_PRINT(AF_IPX);
        CASE_PRINT(AF_APPLETALK);
        CASE_PRINT(AF_PACKET);
        CASE_PRINT(AF_INET6);
        CASE_PRINT(AF_RDS);
        CASE_PRINT(AF_DECnet);
        CASE_PRINT(AF_NETLINK);
        CASE_PRINT(AF_KEY);
        CASE_PRINT(AF_PPPOX);
        CASE_PRINT(AF_CAN);
        CASE_PRINT(AF_BLUETOOTH);
    default: printf("AF_???");
    }
    printf(", ");
    switch (uregs->rsi) {
        CASE_PRINT(SOCK_STREAM);
        CASE_PRINT(SOCK_DGRAM);
        CASE_PRINT(SOCK_SEQPACKET);
        CASE_PRINT(SOCK_RAW);
        CASE_PRINT(SOCK_RDM);
        CASE_PRINT(SOCK_PACKET);
        default: printf("SOCK_??? (%d)", uregs->rsi); 
    }
    printf(", ");

    if (uregs->r10) {
        if (uregs->r10 & SOCK_NONBLOCK) printf("SOCK_NONBLOCK");
        if (uregs->r10 & SOCK_CLOEXEC) printf(" SOCK_CLOEXEC");
    } else {
        printf("0");
    }

    printf(")");
}
void ioctl_print(struct user_regs_struct *uregs) { syscall_printf("ioctl(%d, %d, %p)", uregs->rdi, uregs->rsi, uregs->r10); }

void open_print(struct user_regs_struct *uregs) {
    char buf[128];
    read_string(buf, 128, (void*)uregs->rdi);
    syscall_printf("open(%s, %d)", buf, uregs->rsi);
}

void lstat_print(struct user_regs_struct *uregs) {
    char buf[128];
    read_string(buf, 128, (void*)uregs->rdi);
    syscall_printf("lstat(%s, %p)", buf, uregs->rsi);
}



void mmap_print(struct user_regs_struct *uregs) {
    // mmap is special
    struct mmap_struct {
        void *addr;
        size_t len;
        int prot;
        int flags;
        int filedes;
        off_t off;
    };

    struct mmap_struct m;
    size_t sz = sizeof(struct mmap_struct);
    unsigned char *mptr = (unsigned char *)&m;
    for (size_t i = 0; i < sz; i += sizeof(long)) {
        long word = ptrace(PTRACE_PEEKDATA, child, (void *)((uintptr_t)uregs->rdi + i), NULL);
        memcpy(mptr + i, &word, (i + sizeof(long) > sz) ? (sz - i) : sizeof(long));
    }

    syscall_printf("mmap(%p, %d, %d, ", m.addr, m.len, m.prot);

    // Print flags as string
    int first = 1;
    if (m.flags & 0x01) { printf("\033[33mMAP_SHARED\033[0m"); first = 0; }
    if (m.flags & 0x02) { if (!first) printf("|"); printf("\033[33mMAP_PRIVATE\033[0m"); first = 0; }
    if (m.flags & 0x10) { if (!first) printf("|"); printf("\033[33mMAP_FIXED\033[0m"); first = 0; }
    if (m.flags & 0x20) { if (!first) printf("|"); printf("\033[33mMAP_ANONYMOUS\033[0m"); first = 0; }
    if (first) printf("%d", m.flags);

    printf(", \033[36m%d\033[0m, \033[36m%lld\033[0m)", m.filedes, (long long)m.off);
}