/**
 * @file libpolyhedron/libc/main.c
 * @brief libc startup code
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of reduceOS.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <unistd.h>
#include <stdlib.h>
#include <string.h>

/* Environment */
char **environ = NULL;
int envc = 0;

// TODO: Init array support, mark as constructor - got a lot to do

void __create_environ(char **envp) {
    // First calculate envc
    char **envpp = envp;
    while (*envpp++) {
        envc++;
    }

    // Now start copying
    environ = malloc(envc * sizeof(char*));
    for (int i = 0; i < envc; i++) {
        environ[i] = strdup(envp[i]);
    }
}

void __libc_main(int argc, char **argv, char **envp, int (*main)(int, char**)) {
    // TODO: Call constructors 
    __create_environ(envp);
    exit(main(argc, argv));
}