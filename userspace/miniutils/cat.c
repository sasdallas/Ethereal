/**
 * @file userspace/miniutils/cat.c
 * @brief cat
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
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/stat.h>

/**
 * @brief Usage
 */
void usage() {
    printf("Usage: cat [OPTION]... [FILE]...\n");
    printf("Concatenates FILE(s) to standard output\n");
    exit(EXIT_SUCCESS);
}


/**
 * @brief Version
 */
void version() {
    printf("cat (Ethereal miniutils) 1.00\n");
    printf("Copyright (C) 2025 The Ethereal Development Team\n");
    exit(EXIT_SUCCESS);
}

/**
 * @brief Main
 */
int main(int argc, char *argv[]) {
    struct option longopts[] = {
        { .name = "help", .has_arg = no_argument, .flag = NULL, .val = 'h' },
        { .name = "version", .has_arg = no_argument, .flag = NULL, .val = 'v' },
        { .name = NULL, .has_arg = no_argument, .flag = NULL, .val = 0 },
    };

    int ch;
    int index;

    // pointless getopt for now
    while ((ch = getopt_long(argc, argv, "hv?", (const struct option*)&longopts, &index)) != -1) {
        if (!ch && longopts[index].flag == NULL) ch = longopts[index].val;
        switch (ch) {
            case 'v':
                version();
                break;
            case 'h':
            default:
                usage();
        }
    }

    if (argc-optind == 0) {
        while (1) {
            // We were given no argument, read from stdin file descriptor
            char buf[4096] = { 0 };
            ssize_t r = read(STDIN_FILENO, buf, 4096);

            // If we didn't get anything then return
            if (r < 0) {
                fprintf(stderr, "cat: stdin: %s\n", strerror(errno));
                return 1;
            }

            if (!r) return 0;

            write(STDOUT_FILENO, buf, r);
        }

    }


    int return_value = 0;
    for (int i = optind; i < argc; i++) {
        int fd = STDIN_FILENO;

        // Is this trying to read from stdin?
        if (strcmp(argv[i], "-")) {
            // No, open the file requested
            char *filename = argv[i];
            if ((fd = open(filename, O_RDONLY)) < 0) {
                fprintf(stderr, "cat: %s: %s\n", filename, strerror(errno));
                return_value = 1;
                continue;
            }

            // Get stat
            struct stat st;
            if ((fstat(fd, &st)) < 0) {
                fprintf(stderr, "cat: stat: %s\n", strerror(errno));
                return_value = 1;
                continue;
            }

            if (S_ISDIR(st.st_mode)) {
                fprintf(stderr, "cat: %s: Is a directory\n", filename);
                return_value = 1;
                continue;
            }
        }

        // Enter read loop
        while (1) {
            char buf[4096] = { 0 };
            ssize_t r = read(fd, buf, 4096);
            
            // Error
            if (r < 0) {
                fprintf(stderr, "cat: %s: %s\n", (fd == STDIN_FILENO ? "stdin" : argv[i]), strerror(errno));
                return_value = 1;
                break;
            }

            if (!r) break; // Done

            write(STDOUT_FILENO, buf, r);
        }
    }

    return return_value;
}