/**
 * @file userspace/init/init.c
 * @brief Basic shell, garbage 
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <errno.h>


int main(int argc, char *argv[]) {
    // Are we *really* init?
    if (getpid() != 0) {
        printf("init can only be launched by Ethereal\n");
        return 0;
    }

    // Open files
    int stdin = open("/device/null", O_RDONLY);
    int stdout = open("/device/console", O_RDWR);
    int stderr = open("/device/log", O_RDWR);

    if (stdin != STDIN_FILENO) dup2(stdin, STDIN_FILENO);
    if (stdout != STDOUT_FILENO) dup2(stdout, STDOUT_FILENO);
    if (stderr != STDERR_FILENO) dup2(stderr, STDERR_FILENO); 

    // Setup environment variables
    putenv("PATH=/usr/bin/:/device/initrd/usr/bin/:"); // TEMP

    printf("\nWelcome to the \033[35mEthereal Operating System\033[0m!\n\n");

    // Read kernel command line
    FILE *f = fopen("/kernel/cmdline", "r");
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *cmdline = malloc(size);
    fread(cmdline, size, 1, f);


    pid_t cpid = fork();

    if (!cpid) {
        if (strstr(cmdline, "--old-kernel-terminal")) {
            // Oof, here we go.
            char *nargv[3] = { "terminal", NULL };
            execvpe("terminal", nargv, environ);

            printf("ERROR: Failed to launch terminal process: %s\n", strerror(errno));
        }

        if (strstr(cmdline, "--single-user")) {
            // Launch single-user mode
            char *nargv[3] = { "termemu", "-f", NULL };
            execvpe("termemu", nargv, environ);

            printf("ERROR: Failed to launch terminal process: %s\n", strerror(errno));
        }

        char *nargv[3] = { "/device/initrd/usr/bin/celestial", NULL };
        execvpe("/device/initrd/usr/bin/celestial", nargv, environ);
    
        printf("ERROR: Failed to launch Celestial process: %s\n", strerror(errno));
        return 1;
    }


    while (1) {
        waitpid(-1, NULL, 0);
    }

    return 0;
}
