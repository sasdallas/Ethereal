/**
 * @file libpolyhedron/unistd/system.c
 * @brief system
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>
#include <stdlib.h>

int system(const char *command) {
    pid_t cpid = fork();
    if (!cpid) {
        const char *args[] = {
            "/usr/bin/essence",
            "-c",
            command,
            NULL
        };
        execvpe("/usr/bin/essence", args, environ);
        exit(EXIT_FAILURE);
    }

    int wstatus;
    waitpid(cpid, &wstatus, 0);
    return WEXITSTATUS(wstatus);
}