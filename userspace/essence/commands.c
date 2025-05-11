/**
 * @file userspace/essence/commands.c
 * @brief Basic Essence commands
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include "essence.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int cd(int argc, char *argv[]) {
    char *cd_path = getenv("HOME");
    if (argc > 1) cd_path = argv[1];
    if (!cd_path) cd_path = "/";

    chdir(cd_path);
    return 0;
}

int env(int argc, char *argv[]) {
    char **envp = environ;
    while (*envp) {
        printf("%s\n", *envp);
        envp++;
    }

    return 0;
}

int export(int argc, char *argv[]) {
    if (argc == 1) return env(argc, argv);
    putenv(argv[1]);
    return 0;
}