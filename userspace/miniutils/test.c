/**
 * @file userspace/miniutils/test.c
 * @brief test
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
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    // we are symlinked to "[" for shell script compat.
    if (!strcmp(argv[0], "[")) {
        if (argc && strcmp(argv[argc-1], "]")) {
            return fprintf(stderr, "[: missing ']' in statement\n");
        }

        argc--;
    }

    if (argc < 2) {
        return 1;
    }

    if (argc == 2) {
        return (strlen(argv[1]) > 0) ? 0 : 1;
    }

    if (argc == 3) {
        char *op = argv[1];
        char *target = argv[2];
        struct stat st;

        if (strcmp(op, "-e") == 0) {
            return (access(target, F_OK) == 0) ? 0 : 1;
        }
        
        if (strcmp(op, "-d") == 0) {
            if (stat(target, &st) == 0 && S_ISDIR(st.st_mode)) {
                return 0;
            }
            return 1;
        }

        if (strcmp(op, "-f") == 0) {
            if (stat(target, &st) == 0 && !S_ISDIR(st.st_mode)) {
                return 0;
            }
            return 1;
        }

        if (strcmp(op, "-r") == 0) {
            return (access(target, R_OK) == 0) ? 0 : 1;
        }

        if (strcmp(op, "-z") == 0) {
            return (strlen(target) == 0) ? 0 : 1;
        }

        if (strcmp(op, "-n") == 0) {
            return (strlen(target) > 0) ? 0 : 1;
        }

        fprintf(stderr, "test: unknown unary operator %s\n", op);
        return 2;
    }

    if (argc == 4) {
        const char *val1 = argv[1];
        const char *op = argv[2];
        const char *val2 = argv[3];

        if (strcmp(op, "=") == 0 || strcmp(op, "==") == 0) {
            return (strcmp(val1, val2) == 0) ? 0 : 1;
        }

        if (strcmp(op, "!=") == 0) {
            return (strcmp(val1, val2) != 0) ? 0 : 1;
        }

        long n1 = strtol(val1, NULL, 10);
        long n2 = strtol(val2, NULL, 10);
        if (strcmp(op, "-eq") == 0) return (n1 == n2) ? 0 : 1;
        if (strcmp(op, "-ne") == 0) return (n1 != n2) ? 0 : 1;
        if (strcmp(op, "-gt") == 0) return (n1 > n2)  ? 0 : 1;
        if (strcmp(op, "-ge") == 0) return (n1 >= n2) ? 0 : 1;
        if (strcmp(op, "-lt") == 0) return (n1 < n2)  ? 0 : 1;
        if (strcmp(op, "-le") == 0) return (n1 <= n2) ? 0 : 1;

        fprintf(stderr, "test: unknown binary operator %s\n", op);
        return 2;
    }

    fprintf(stderr, "test: too many arguments\n");
    return 2;
}
