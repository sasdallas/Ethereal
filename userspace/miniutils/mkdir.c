/**
 * @file userspace/miniutils/mkdir.c
 * @brief mkdir
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
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>


void usage() {
    printf("Usage: mkdir [OPTION]... [DIRECTORY]\n");
    printf("Creates the DIRECTORY(ies) if they do not already exist\n");
    exit(1);   
}

void version() {
    printf("mkdir (Ethereal miniutils) 1.00\n");
    printf("Copyright (C) 2025 The Ethereal Development Team\n");
    exit(1);
}

int main(int argc, char *argv[]) {
    int verbose = 0;
    mode_t mode = 0777; 
    int parents = 0;

    struct option options[] = {
        { .name = "mode", .has_arg = required_argument, .flag = NULL, .val = 'm' },
        { .name = "verbose", .has_arg = no_argument, .flag = NULL, .val = 'v' },
        { .name = "help", .has_arg = no_argument, .flag = NULL, .val = 'h' },
        { .name = "version", .has_arg = no_argument, .flag = NULL, .val = 'd' },
        { .name = "parents", .has_arg = no_argument, .flag = NULL, .val = 'p' },
        { .name = NULL, .has_arg = no_argument, .flag = NULL, .val = 0 },
    };

    int index;
    int ch;
    while ((ch = getopt_long(argc, argv, "m:vp", (const struct option*)&options, &index)) != -1) {
        if (!ch && options[index].flag == NULL) {
            ch = options[index].val;
        }

        switch (ch) {
            case 'm':
                fprintf(stderr, "mkdir: mode not supported\n");
                return 1;

            case 'v':
                verbose = 1;
                break;
            
            case 'p':
                parents = 1;
                break;

            case 'd':
                version();
                break;

            case 'h':
            default:
                usage();
        }
    }

    if (!(argc-optind)) {
        fprintf(stderr, "mkdir: missing operand\nTry 'mkdir --help' for more information.\n");
        return 1;
    } 

    int retval = 0;
    for (int i = optind; i < argc; i++) {
        // Start parsing
        if (parents) {
            // We need to handle each part of the path
            char *path = strdup(argv[i]);
            char *c = strchr(path+1, '/');
            while (c) {
                *c = 0;
                int r = mkdir(path, mode);
                if (r < 0) {
                    if (errno != EEXIST) {
                        // Problem
                        fprintf(stderr, "mkdir: %s: %s\n", path, strerror(errno));
                        retval = 1;
                        break;
                    }
                }

                if (verbose) printf("mkdir: created directory \'%s\'\n", path);
                *c = '/';
                c = strchr(c+1, '/');
            }

            // Create the final directory
            int r = mkdir(path, mode);
            if (r < 0) {
                if (errno != EEXIST) {
                    // Problem
                    fprintf(stderr, "mkdir: %s: %s\n", path, strerror(errno));
                    retval = 1;
                    free(path);
                    continue;
                }
            }

            if (verbose) printf("mkdir: created directory \'%s\'\n", path);
            free(path);
        } else {
            int r = mkdir(argv[i], mode);
            if (r < 0) {
                fprintf(stderr, "mkdir: %s: %s\n", argv[i], strerror(errno));
                retval = 1;
            } else if (verbose) {
                printf("mkdir: created directory \'%s\'\n", argv[i]);
            }
        }
    }

    return retval;
}