/**
 * @file userspace/ls/ls.c
 * @brief ls command
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal operating system.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <dirent.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

void help() {
    printf("Usage: ls [OPTION]... [FILE]...\n");
    printf("List information about the FILEs (the current directory by default)\n");
    exit(EXIT_SUCCESS);
}

void version() {
    printf("ls (Ethereal miniutils) 1.00\n");
    printf("Copyright (C) 2025 The Ethereal Development Team\n");
    exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
    if (argc > 1 && !strcmp(argv[1], "--help")) help();
    if (argc > 1 && !strcmp(argv[1], "--version")) version();

    char *path = (argc > 1) ? argv[1] : ".";

    DIR *dirp = opendir(path);
    if (!dirp) {
        printf("ls: Directory \"%s\" could not be opened - %s\n", path, strerror(errno));
        return 1;
    }

    struct dirent *ent;

    while ((ent = readdir(dirp)) != NULL) {
        printf("%s\n", ent->d_name);
    }

    return 0;
}