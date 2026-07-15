/**
 * @file userspace/miniutils/cmdline.c
 * @brief Stupid command line checker
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

void usage() {
    fprintf(stderr, "Usage: cmdline [-hvV] [OPTION]\n");
    fprintf(stderr, "Utility to check kernel command line.\n\n");
    fprintf(stderr, " -h    Show help message and exit\n");
    fprintf(stderr, " -v    Get the value of an option, or return 1\n");
    fprintf(stderr, " -V    Show version and exit\n");
    exit(1);
}

void version() {
    fprintf(stderr, "cmdline (Ethereal miniutils) 1.00\n");
    fprintf(stderr, "Copyright (C) 2026 The Ethereal Development Team\n");
    exit(1);
}

int main(int argc, char *argv[]) {
    char *target_key = NULL;

    if (argc > 1 && argv[argc - 1][0] == '-' && argv[argc - 1][1] == '-') {
        target_key = argv[argc - 1];
        argc--;
    }

    FILE *f = fopen("/system/cmdline", "r");
    if (!f) return 0;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    char *cmdline = malloc(sz + 1);
    fseek(f, 0, SEEK_SET);

    size_t read_bytes = fread(cmdline, 1, sz, f);
    cmdline[read_bytes] = '\0';
    fclose(f);

    if (read_bytes > 0 && (cmdline[read_bytes - 1] == '\n' || cmdline[read_bytes - 1] == '\r')) {
        cmdline[read_bytes - 1] = '\0';
    }

    int value = 0;
    int ch;
    opterr = 0;

    while ((ch = getopt(argc, argv, "hvV")) != -1) {
        if (ch == '?') break;

        switch (ch) {
            case 'v':
                value = 1;
                break;
            case 'V':
                version();
                free(cmdline);
                return 0;
            case 'h':
            default:
                usage();
                free(cmdline);
                return 0;
        }
    }

    if (!target_key) {
        if (optind < argc) {
            target_key = argv[optind];
        } else {
            printf("%s\n", cmdline);
            free(cmdline);
            return 0;
        }
    }

    char *opt = strstr(cmdline, target_key);

    if (opt) {
        if (!value) {
            free(cmdline);
            return 0; 
        }

        char *value_ptr = opt + strlen(target_key);

        if (*value_ptr != '=') {
            free(cmdline);
            return 2; 
        }

        value_ptr++;

        char *nextarg = strchrnul(value_ptr, ' ');
        
        char saved_char = *nextarg;
        *nextarg = '\0';

        printf("%s\n", value_ptr);

        *nextarg = saved_char;
        free(cmdline);
        return 0;
    } else {
        free(cmdline);
        return 1;
    }
}