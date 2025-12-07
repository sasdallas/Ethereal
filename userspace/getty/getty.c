/**
 * @file userspace/getty/getty.c
 * @brief getty clone
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <termios.h>
#include <pwd.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <asm/ioctls.h>
#include <sys/utsname.h>
#include <errno.h>
#include <time.h>
#include <ethereal/version.h>

char *prog_name = NULL;
char *tty = NULL;

int setup_tty(char *_tty) {
    tty = _tty;
    close(1);
    close(2);
    errno = 0;

    if (!strcmp(tty, "-")) {
        // User wants to use stdin
    } else {
        // Open the device from /device/
        if (chdir("/device/") < 0) {
            perror("chdir");
            return 1;
        }

        struct stat st;
        if (stat(tty, &st) < 0) {
            perror("stat");
            return 1;
        }

        if ((st.st_mode & S_IFMT) != S_IFCHR) {
            fprintf(stderr, "%s: %s: not a character device\n", prog_name, tty);
            return 1;
        }

        close(0);

        if (open(tty, O_RDWR | O_NONBLOCK) != 0) {
            perror("open");
            return 1;
        }
    }

    // Duplicate fds
    if (dup(0) != 1 || dup(0) != 2) {
        fprintf(stderr, "%s: dup error: %s\n", prog_name, strerror(errno));
        return 1;
    } 

    return 0;
}

void setup_tios(int baud_rate) {
    struct termios tios;
    tios.c_cflag = CS8 | HUPCL | CREAD | baud_rate; // Taken from agetty source code (https://kernel.googlesource.com/pub/scm/utils/util-linux/util-linux/+/v2.7.1/login-utils/agetty.c)
    tios.c_iflag = 0;
    tios.c_lflag = 0;
    tios.c_oflag = 0;
    tios.c_line = 0;
    tios.c_cc[VMIN] = 1;
    tios.c_cc[VTIME] = 0;
    ioctl(STDIN_FILENO, TCSETA, &tios);
    fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL, 0) & ~(O_NONBLOCK));
    setsid();
    int _a = 1;
    ioctl(STDIN_FILENO, TIOCSCTTY, &_a);
    tcsetpgrp(STDIN_FILENO, getpid());
}

void parse_issue_seq(char ch) {
    struct utsname uts;
    uname(&uts);
    switch (ch) {
        case 'b':
            printf("9600"); // TODO
            break;
        case 'd':
        case 't':
            time_t t = time(NULL);
            struct tm *tm = localtime(&t);
            char *weekday[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

            char *month[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
            if (ch == 'd') {
                // print date
                printf("%s %s %d  %d", weekday[tm->tm_wday], month[tm->tm_mon], tm->tm_mday, tm->tm_year < 70 ? tm->tm_year + 2000 : tm->tm_year + 1900);
            } else {
                // print time
                printf("%02d:%02d:%02d", tm->tm_hour, tm->tm_min, tm->tm_sec);
            }
            break;

        case 'l':
            printf(tty);
            break;
        
        case 'V':
            const ethereal_version_t *ver = ethereal_getVersion();
            printf("%d.%d.%d", ver->version_major, ver->version_minor, ver->version_lower);
            break;
        
        case 'm':
            printf("%s", uts.machine);
            break;
        
        case 'n':
            printf("%s", uts.nodename);
            break;
        
        case 'r':
            printf("%s", uts.release);
            break;
        
        case 's':
            printf("%s", uts.sysname);
            break;

        case 'v':
            printf("%s", uts.version);
            break;
        
        case 'U':
            printf("1 user"); // TEMPORARY
            break;

        default:
            putchar(ch);
            break;
    }
}

void show_issue() {
    FILE *issue = fopen("/etc/issue", "r");
    if (!issue) return;

    int ch;
    while ((ch = fgetc(issue)) != EOF) {
        if (ch == '\\') {
            parse_issue_seq(fgetc(issue));
        } else {
            putchar(ch);
        }
    }


    fclose(issue);
    fflush(stdout);
}


int main(int argc, char *argv[]) {
    if (getuid() != 0) {
        fprintf(stderr, "%s: root permission is required\n", argv[0]);
        return 1;
    }


    // Open the TTY
    prog_name = argv[0];
    if (setup_tty(argc > 1 ? argv[1] : "-")) {
        return 1;
    }

    // Setup the termios
    setup_tios(9600);

    // /etc/issue
    show_issue();

    // Execute login
    char *args[] = { "login", NULL };
    execvpe("login", args, environ);
    fprintf(stderr, "%s: error starting login process: %s\n", argv[0], strerror(errno));
    return 1;
}