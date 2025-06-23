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


int main(int argc, char *argv[]) {
    // Are we *really* init?
    if (getpid() != 0) {
        printf("init can only be launched by Ethereal\n");
        return 0;
    }

    // Check to see if argv[1] == "headless"
    if (argc > 1 && !strcmp(argv[1], "headless")) {
        open("/device/ttyS0", O_RDONLY);
        open("/device/ttyS0", O_RDWR);
        open("/device/ttyS0", O_RDWR);
    } else {
        open("/device/stdin", O_RDONLY);
        open("/device/kconsole", O_RDWR);
        open("/device/kconsole", O_RDWR);
    }




    putenv("PATH=/usr/bin/:/device/initrd/usr/bin/:"); // TEMP

    printf("Welcome to Ethereal\n");
    if (!fork()) {
        argv[0] = "/device/initrd/usr/bin/migrate";
        execvpe("/device/initrd/usr/bin/migrate", (const char**)argv, environ);
    } else {
        waitpid(-1, NULL, 0);
    }

    printf("Initializing shell...\n");
    argv[0] = "/device/initrd/usr/bin/essence";
    execvpe("/device/initrd/usr/bin/essence", (const char**)argv, environ);
    return 0;
}
