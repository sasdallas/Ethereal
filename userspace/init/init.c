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

    // Open files
    open("/device/stdin", O_RDONLY);
    open("/device/log", O_RDWR);
    open("/device/log", O_RDWR);

    putenv("PATH=/usr/bin/:/device/initrd/usr/bin/:"); // TEMP

    printf("Welcome to the Ethereal Operating System\n");

    system("/etc/init.d/01_welcome.sh");

    printf("Initializing shell...\n");
    
    pid_t cpid = fork();

    if (!cpid) {
        char *nargv[3] = { "/device/initrd/usr/bin/celestial",  "-d", NULL };
        execvpe("/device/initrd/usr/bin/celestial", (const char**)nargv, environ);
    
        printf("ERROR: Failed to launch terminal process: %s\n", strerror(errno));
        return 1;
    }


    while (1) {
        waitpid(-1, NULL, 0);
    }

    return 0;
}
