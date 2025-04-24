/**
 * @file userspace/miniutils/cat.c
 * @brief cat
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void help() {
    printf("Usage: cat [OPTION]... [FILE]...\n");
    printf("Concatenates FILE(s) to standard output\n");
    exit(EXIT_SUCCESS);
}

void version() {
    printf("cat (Ethereal miniutils) 1.00\n");
    printf("Copyright (C) 2025 The Ethereal Development Team\n");
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {

    // Check arguments
    if (argc > 1 && !strcmp(argv[1], "--help")) {
        help();
    }

    if (argc > 1 && !strcmp(argv[1], "--version")) {
        version();
    }

    // Stat to make sure
    if (argc > 1) {
        struct stat st;
        if (stat(argv[1], &st) < 0){ 
            printf("cat: %s: %s\n", argv[1], strerror(errno));
            return 1;
        }
    }

    FILE *f = NULL;
    if (argc < 1) {
        f = stdin;
    } else {
        f = fopen(argv[1], "r");
        if (!f) {
            printf("cat: %s: %s\n", argv[1], strerror(errno));
            return 1;
        }
    }

    fseek(f, 0, SEEK_END);
    long flen = ftell(f);
    fseek(f, 0, SEEK_SET);

    // Chunk the file up
    char *buffer = malloc(4097);
    memset(buffer, 0, 4097);
    for (int i = 0; i < flen; i += 4096) {
        size_t l = fread(buffer, 1, 4096, f);
        buffer[l] = 0;

        printf("%s", buffer);
    }

    fflush(stdout);
    free(buffer);

    return 0;
}