/**
 * @file userspace/miniutils/ls.c
 * @brief ls command
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <dirent.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <sys/stat.h>
#include <pwd.h>
#include <errno.h>
#include <termios.h>
#include <sys/ioctl.h>

/* ls flags */
int all = 0;
int almost_all = 0;
int list = 0;

/* Running on a TTY */
int is_tty = 0;

/* Exit status */
int exit_status = 0;

/* Print directory names at the top */
int use_dir_names = 0;

/* For permissions */
#define PRINT_MODE(m, c) { if (st.st_mode & m) { putchar(c); } else { putchar('-'); }; }

/* Column size */
int column_size = 1;
int columns = 0;
int term_width = 0;

/* Colors */
#define COLOR_EXECUTABLE                "\033[1;32m"
#define COLOR_DIRECTORY                 "\033[1;34m"
#define COLOR_SYMLINK                   "\033[1;36m"
#define COLOR_DEVICE                    "\033[1;33;40m"
#define COLOR_SETUID                    "\033[37;41m"

void help() {
    printf("Usage: ls [OPTION]... [FILE]...\n");
    printf("List information about the FILEs (the current directory by default)\n");
    exit(EXIT_FAILURE);
}

void version() {
    printf("ls (Ethereal miniutils) 1.00\n");
    printf("Copyright (C) 2025 The Ethereal Development Team\n");
    exit(EXIT_FAILURE);
}


void print_entry(char *ent) {
    if (!all && !almost_all) {
        if (ent[0] == '.') return;
    } else if (!all) {
        if (!strcmp(ent, ".") || !strcmp(ent, "..")) return; 
    }

    // Get stat data
    struct stat st;
    if (stat(ent, &st) < 0) {
        fprintf(stderr, "ls: %s: %s\n", ent, strerror(errno));
        return;
    }

    // Print information first
    if (list) {
        // Get type of file and print that
        if (S_ISLNK(st.st_mode)) {
            putchar('l');
        } else if (S_ISDIR(st.st_mode)) {
            putchar('d');
        } else if (S_ISBLK(st.st_mode)) {
            putchar('b');
        } else if (S_ISCHR(st.st_mode)) {
            putchar('c');
        } else if (S_ISFIFO(st.st_mode)) {
            putchar('f');
        } else if (S_ISSOCK(st.st_mode)) {
            putchar('s');
        } else {
            putchar('-');
        }

        // First, do the permissions printing
        PRINT_MODE(S_IRUSR, 'r');
        PRINT_MODE(S_IWUSR, 'w');
        PRINT_MODE(S_IXUSR, 'x');
        PRINT_MODE(S_IRGRP, 'r');
        PRINT_MODE(S_IWGRP, 'w');
        PRINT_MODE(S_IXGRP, 'x');
        PRINT_MODE(S_IROTH, 'r');
        PRINT_MODE(S_IWOTH, 'w');
        PRINT_MODE(S_IXOTH, 'x');
        printf(" ");

        printf("%-2d ", st.st_nlink);

        struct passwd *p = getpwuid(st.st_uid);
        if (p) {
            printf("%-10s   ", p->pw_name);
        } else {
            printf("%10ld   ", st.st_uid);
        }

        printf("%10ld ", st.st_size);
    }


    // We are a TTY, print some colors
    if (S_ISBLK(st.st_mode) || S_ISCHR(st.st_mode) || S_ISFIFO(st.st_mode)) {
        printf(COLOR_DEVICE);    
    } else if (S_ISDIR(st.st_mode)) {
        printf(COLOR_DIRECTORY);
    } else if (st.st_mode & S_ISUID) {
        printf(COLOR_SETUID);
    } else if (st.st_mode & 0111) {
        printf(COLOR_EXECUTABLE);
    }
    
    if (!is_tty || list) {
        printf("%s%s\n", ent, is_tty ? "\033[0m" : "");
    } else {

        printf("%s\033[0m", ent);
    
        for (int i = column_size - strlen(ent); i > 0; i--) putchar(' ');
    }
}


int sort_fn(const void *s1, const void *s2) {
    return strcmp((const char*)s1, (const char*)s2);
}

void list_directory(char *dir) {
    DIR *d = opendir(dir);
    if (!d) {
        fprintf(stderr, "ls: cannot access \'%s\': %s\n", dir, strerror(errno));
        exit_status = 2;
        return;
    }

    // Change to directory for stat
    chdir(dir);

    // Create an expanding entry list
    char **ent_list = NULL;
    int entry_count = 0;

    // Read directory
    column_size = 0;
    struct dirent *ent = readdir(d);
    while (ent) {
        entry_count++;
        
        if (ent_list) {
            ent_list = realloc(ent_list, sizeof(char*) * entry_count);
        } else {
            ent_list = malloc(sizeof(char*));
        }

        if (is_tty && strlen(ent->d_name) > (size_t)column_size) column_size = strlen(ent->d_name);

        ent_list[entry_count-1] = strdup(ent->d_name);
        ent = readdir(d);
    }

    if (is_tty) {
        column_size++;
        columns = ((term_width - column_size) / (column_size + 2)) + 1;
    }

    // Show name if needed
    if (use_dir_names) printf("%s:\n", dir);

    // Sort the directory entries, if we have any
    if (!entry_count) return;
    qsort(ent_list, entry_count, sizeof(char*), sort_fn);

    // Now let's print them out!
    if (!is_tty || list) {
        // One output per line
        for (int i = 0; i < entry_count; i++) {
            print_entry(ent_list[i]);
        }
    } else {
        // We have columns
        int col = 0;
        for (int i = 0; i < entry_count; i++) {
            print_entry(ent_list[i]);
         
            if (!all && !almost_all) {
                if (ent_list[i][0] == '.') continue;
            } else if (!all) {
                if (!strcmp(ent_list[i], ".") || !strcmp(ent_list[i], "..")) continue; 
            }

            if (strlen(ent_list[i]) == (size_t)column_size) putchar(' ');
            
            col++;
            if (col >= columns) {
                col = 0;
                printf("\n");
            }
        }
        if (col) putchar('\n');
    }
}

int main(int argc, char **argv) {
    struct option options[] = {
        { .name = "all", .has_arg = no_argument, .flag = NULL, .val = 'a' },
        { .name = "almost-all", .has_arg = no_argument, .flag = NULL, .val = 'A' },
        { .name = "help", .has_arg = no_argument, .flag = NULL, .val = 'h' },
        { .name = "version", .has_arg = no_argument, .flag = NULL, .val = 'v' },
    };

    int index;
    int ch;

    while ((ch = getopt_long(argc, argv, "aAl", (const struct option*)options, &index)) != -1) {
        if (!ch && options[index].flag == NULL) {
            ch = options[index].val;
        }

        switch (ch) {
            case 'a':
                all = 1;
                break;

            case 'A':
                almost_all = 1;
                break;

            case 'l':
                list = 1;
                break;

            case 'h':
                help();
                break;

            case 'v':
                version();
                break;
        }
    }

    // Check if we are running on a TTY
    if (isatty(STDOUT_FILENO)) {
        is_tty = 1;

        struct winsize winsz;
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &winsz);

        term_width = winsz.ws_col - 1;
    }

    // Do we specify directories?
    if (argc-optind) {
        use_dir_names = (argc-optind > 2);
        for (int i = optind; i < argc; i++) {
            list_directory(argv[i]);
        }
    } else {
        list_directory(".");
    }

    return exit_status;
}