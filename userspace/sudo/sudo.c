/**
 * @file userspace/sudo/sudo.c
 * @brief sudo
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <pwd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <termios.h>
#include <getopt.h>
#include <stdbool.h>
#include <ctype.h>

/* Whether we are starting a login shell */
bool login = false;

/* Username to login to */
char *user = "root";
struct passwd *pwd = NULL;

/* Command to execute */
char **cmd;

void usage() {
    fprintf(stderr, "Usage: sudo [-h] [-i] [-V] [-u user] [COMMAND]\n");
    fprintf(stderr, "Execute a command as another user\n\n");

    printf(" -h, --help         Display this help message\n");
    printf(" -i, --login        Login shell on target user\n");
    printf(" -u, --user         Run command as specified user name or ID\n");
    printf(" -V, --version      Print the version of sudo\n\n");

    exit(0);
}

void version() {
    printf("sudo version 1.0.0\n");
    printf("Copyright (C) 2026 The Ethereal Development Team\n");
    exit(0);
}

void sudo_ok() {
    if (setgid(pwd->pw_gid) < 0) {
        fprintf(stderr, "sudo: setgid: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (setuid(pwd->pw_uid) < 0) {
        fprintf(stderr, "sudo: setuid: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Prepare environmental variables
    setenv("USER", strdup(pwd->pw_name), 1);
    setenv("HOME", strdup(pwd->pw_dir), 1);
    setenv("SHELL", strdup(pwd->pw_shell), 1);

    if (login) {
        char *argv[] = { getenv("SHELL"), NULL };
        execvp(argv[0], argv);
        fprintf(stderr, "sudo: execvp: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    } else {
        execvp(cmd[0], cmd);
        fprintf(stderr, "sudo: execvp: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
}

char *sudo_get_password(struct passwd *current) {
    // TODO: This will all be moved to a libauth sometime
    FILE *shadow = fopen("/etc/shadow", "r");
    if (!shadow) {
        fprintf(stderr, "sudo: /etc/shadow: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    while (!feof(shadow)) {
        char line[1024];
        fgets(line, 1024, shadow);
        *(strchrnul(line, '\n')) = 0;

        char *colon = strchr(line, ':');
        if (colon == NULL) continue;

        *colon = 0;

        if (strncmp(line, current->pw_name, 1024) == 0) {
            char *password = colon+1;
            
            colon = strchr(password, ':');
            if (!colon) {
                fprintf(stderr, "sudo: corrupt /etc/shadow file, bad entry for %s\n", current->pw_name);
                exit(EXIT_FAILURE);
            }

            *colon = 0;
            if (*password == 0) {
                return NULL;
            } else {
                return strdup(password);
            }
        }
    }

    // the user probably has no password
    return NULL;
}

int sudo_password(struct passwd *current) {
    if (!isatty(STDIN_FILENO)) {
        fprintf(stderr, "sudo: expected a tty on stdin\n");
        return 1;
    }

    char *target_password = sudo_get_password(current);
    if (target_password == NULL) {
        // this account has no password
        return 0;
    }

    int attempts = 3;
    bool ok = false;
    while (attempts--) {
        printf("[sudo] password for %s: ", current->pw_name);
        fflush(stdout);

        struct termios old, new;
        tcgetattr(STDIN_FILENO, &old);
        tcgetattr(STDIN_FILENO, &new);
        new.c_lflag &= ~(ECHO | ICANON);
        new.c_oflag |= ONLCR;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &new);

        char password[1024] = { 0 };
        char *buf = password;
        while (1) {
            int ch = getchar();
            if (ch == EOF) break;
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

            if (isprint(ch)) {
                *buf++ = ch;
                if (buf >= password+512) {
                    break;
                }
                putchar('*');
                fflush(stdout);
            }
        }

        tcsetattr(STDIN_FILENO, TCSAFLUSH, &old);

        if (strlen(password) == strlen(target_password) && !strcmp(target_password, password)) {
            ok = true;
            break;
        }

        sleep(2);
        printf("Sorry, try again.\n");
    }

    if (!ok) {
        fprintf(stderr, "sudo: 3 incorrect password attempts\n");
        return 1;
    }

    return 0;    
}

int main(int argc, char *argv[]) {
    if (geteuid() != 0) {
        fprintf(stderr, "sudo: running as EUID %d and not 0\n", geteuid());
        fprintf(stderr, "sudo: please check that sudo is SUID\n");
        return 1;
    }

    struct option options[] = {
        { .name = "help", .flag = NULL, .has_arg = no_argument, .val = 'h' },
        { .name = "version", .flag = NULL, .has_arg = no_argument, .val = 'V' },
        { .name = "user", .flag = NULL, .has_arg = required_argument, .val = 'u' },
        { .name = "login", .flag = NULL, .has_arg = no_argument, .val = 'i' },
        { 0, 0, 0, 0 },
    };

    int ch;
    int longindex;
    while ((ch = getopt_long(argc, argv, "+hVu:i", (const struct option*)options, &longindex)) != -1) {
        if (!ch) {
            ch = options[longindex].val;
        }

        switch (ch) {
            case 'i':
                login = true;
                break;

            case 'V':
                version();
                break;

            case 'u':
                user = strdup(optarg);
                break;

            default:
                usage();
                break;
        }
    }


    if (!login && (argc-optind) == 0) {
        fprintf(stderr, "sudo: a command was expected but none was given\n");
        fprintf(stderr, "sudo: see sudo --help for more information\n");
        return 1;
    }

    cmd = &argv[optind];
    
    // If we are root, no need to care
    if (getuid() == 0) {
        goto _pass;
    }

    struct passwd *current = getpwuid(getuid());
    if (current == NULL) {
        fprintf(stderr, "sudo: getpwuid failed for calling user\n");
        return 1;
    }

    // Get password
    if (sudo_password(current)) {
        return 1;
    }

    // Validate this user is in the sudoers file
    FILE *f = fopen("/etc/sudoers", "r");
    if (!f) {
        fprintf(stderr, "sudo: /etc/sudoers: %s\n", strerror(errno));
        return 1;
    }

    bool found = false;
    while (!feof(f)) {
        char line[1024];
        fgets(line, 1024, f);
        *(strchrnul(line, '\n')) = 0;
        if (!strncmp(current->pw_name, line, 1024)) {
            found = true;
            break;
        }
    }

    fclose(f);

    if (found == false) {
        fprintf(stderr, "%s is not in the sudoers file.\n", current->pw_name);
        return 1;
    }

_pass:
    pwd = getpwnam(user);
    if (pwd == NULL) {
        fprintf(stderr, "sudo: unknown user %s\n", user);
        return 1;
    }

    sudo_ok();
    return 0;
}
