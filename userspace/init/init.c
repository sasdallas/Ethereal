/**
 * @file userspace/init/init.c
 * @brief Init program
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
#include <dirent.h>

int sort_fn(const void *a, const void *b) { return strcmp(((struct dirent*)a)->d_name, ((struct dirent*)b)->d_name); }

void run_init_scripts() {
    DIR *dir = opendir("/etc/init.d/");
    if (!dir) {
        printf("ERROR: Failed to open /etc/init.d/: %s\n", strerror(errno));
        return;
    }

    struct dirent *entry;
    struct dirent **entries = NULL;
    size_t count = 0;

    // Read all entries
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') {
            continue; // Skip hidden files and "." or ".."
        }
        entries = realloc(entries, sizeof(struct dirent *) * (count + 1));
        entries[count] = malloc(sizeof(struct dirent));
        memcpy(entries[count], entry, sizeof(struct dirent));
        count++;
    }
    closedir(dir);

    // Sort entries
    qsort(entries, count, sizeof(struct dirent *), sort_fn);

    // Execute scripts
    for (size_t i = 0; i < count; i++) {
        char path[256];
        snprintf(path, sizeof(path), "/etc/init.d/%s", entries[i]->d_name);

        pid_t pid = fork();
        if (pid == 0) {
            execl(path, path, NULL);
            printf("ERROR: Failed to execute %s: %s\n", path, strerror(errno));
            exit(1);
        } else if (pid > 0) {
            int status;
            waitpid(pid, &status, 0);
            if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
                printf("Script %s exited with status %d\n", entries[i]->d_name, WEXITSTATUS(status));
            }
        } else {
            printf("ERROR: Failed to fork for %s: %s\n", entries[i]->d_name, strerror(errno));
        }

        free(entries[i]);
    }

    free(entries);
}

int main(int argc, char *argv[]) {
    // Are we *really* init?
    if (getpid() != 0) {
        printf("init can only be launched by Ethereal\n");
        return 0;
    }

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    // Open files
    open("/device/null", O_RDONLY);
    open("/device/console", O_RDWR);
    open("/device/log", O_RDWR);

    // Setup environment variables
    putenv("PATH=/usr/bin/:/device/initrd/usr/bin/:"); // TEMP
    setbuf(stdout, NULL);

    printf("\nWelcome to the \033[35mEthereal Operating System\033[0m!\n\n");

    run_init_scripts();

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

        char *nargv[3] = { "/device/initrd/usr/bin/celestial",  NULL };
        execvpe("/device/initrd/usr/bin/celestial", nargv, environ);
    
        printf("ERROR: Failed to launch Celestial process: %s\n", strerror(errno));
        return 1;
    }


    while (1) {
        waitpid(-1, NULL, 0);
    }

    return 0;
}
