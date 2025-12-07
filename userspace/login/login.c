/**
 * @file userspace/login/login.c
 * @brief login program
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
#include <pwd.h>
#include <termios.h>
#include <string.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include <sys/ioctl_ethereal.h>
#include <unistd.h>
#include <sys/wait.h>

char username[512];
char password[512];

void usage() {
    printf("Usage: login [-H] [<username>]\n");
    printf("Begin a session on the system.\n\n");

    printf(" -H         Display this help\n");
    printf(" -v         Display version\n");
    exit(0);
}

void version() {
    printf("login (Ethereal miniutils) 1.0.0\n");
    printf("Copyright (C) 2025 The Ethereal Development Team\n");
    exit(0);
}

void show_prompt() {
    char hostname[256];
    gethostname(hostname, 256);

    write(1, hostname, strlen(hostname));
    write(1, " login: ", sizeof(" login: ")-1);
}

void exec_fork(uid_t uid) {
    pid_t p = fork();
    if (!p) {
        ioctl(STDIN_FILENO, IOCTLTTYLOGIN, &uid);
        setsid();
        int a = 1;
        ioctl(STDIN_FILENO, TIOCSCTTY, &a);
        tcsetpgrp(STDIN_FILENO, getpid());

        setgid(uid);
        setuid(uid);
        // TODO: Set groups and set vars

        char *args[] = { "essence", NULL };
        execvp("essence", args);
        exit(1);
    } else {
        int r;
        do {
            r = waitpid(p, NULL, 0);
        } while (r < 0);
    }
}

void prompt_loop() {
    while (1) {
        // Read username
        show_prompt();
        
        if (!fgets(username, 512, stdin)) {
            clearerr(stdin);
            fprintf(stderr, "\n");
            sleep(2);
            fprintf(stderr, "\nLogin failed.\n");
            continue;
        }

        username[strlen(username)-1] = 0;

        printf("Password: ");
        fflush(stdout);
        
        struct termios old, new;
        tcgetattr(STDIN_FILENO, &old);
        tcgetattr(STDIN_FILENO, &new);
        new.c_lflag &= ~(ECHO | ICANON);
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &new);

        char *buf = password;
        while (1) {
            int ch = getchar();
            if (ch == '\n') { putchar('\n'); break; }
            if (ch == '\b' || ch == 0x7f) {
                if (buf != password) {
                    buf--;
                    *buf = 0;
                    printf("\b \b");
                    fflush(stdout);
                }
                continue;
            } 
            *buf++ = ch;
            if (buf >= password+512) {
                break;
            }
            putchar('*');
            fflush(stdout);
        }

        *buf = 0;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &old);

        // TODO: Auth checking
        struct passwd *pw = getpwnam(username);
        if (!pw) {
            clearerr(stdin);
            fprintf(stderr, "\n");
            sleep(2);
            fprintf(stderr, "\nLogin failed.\n");
            continue;
        }

        exec_fork(pw->pw_uid);
        break;
    }

    
}

int main(int argc, char *argv[]) {
    int ch;
    while ((ch = getopt(argc, argv, "Hv")) != -1) {
        switch (ch) {
            case 'v':
                version();
                break;

            case 'H':
            default:
                usage();
        }
    }

    char *user = NULL;
    if (argc-optind) {
        user = argv[optind];
    
        struct passwd *pw = getpwnam(user);
        if (pw) {
            exec_fork(pw->pw_uid);
            return 1;
        } else {
            fprintf(stderr, "%s: %s: no such user\n", argv[0], user);
            return 1;
        }
    } 

    prompt_loop();

    return 0;
}