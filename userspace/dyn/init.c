/**
 * @file userspace/dyn/init.c
 * @brief Dynamic linker test
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
#include <unistd.h>

int main(int argc, char *argv[]) {
    printf("Hi from dynamically loaded program!\n");
    printf("There are %d argument(s) that we were given:\n", argc);
    for (int i = 0; i < argc; i++) {
        printf("\t%s\n", argv[i]);
    }

    printf("Environmental variables:\n");

    char **envp = environ;
    while (*envp) {
        printf("\t%s\n", envp);
        envp++;
    }

    printf("Quitting\n");
    return 0;
}