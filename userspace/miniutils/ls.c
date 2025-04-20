/**
 * @file userspace/ls/ls.c
 * @brief ls command
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of reduceOS.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <dirent.h>
#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv) {
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