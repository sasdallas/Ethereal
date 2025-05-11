/**
 * @file userspace/essence/essence.c
 * @brief Essence shell
 * 
 * Essence is a bash-like shell designed for Ethereal
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include "essence.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Essence prompt */
static char __essence_prompt[128];

/**
 * @brief Essence command
 */
int essence_command(int argc, char *argv[]) {
    printf("Essence (Ethereal Operating System) v%d.%d.%d\n", ESSENCE_VERSION_MAJOR, ESSENCE_VERSION_MINOR, ESSENCE_VERSION_LOWER);
    printf("Copyright (C) 2025 Ethereal Development Team\n");
    return 0;
}

/**
 * @brief Get the essence prompt
 */
char *essence_getPrompt() {
    char *cwd = getcwd(NULL, 512);
    snprintf(__essence_prompt, 128, "%s$ ", cwd);
    free(cwd);
    return __essence_prompt;
}

/**
 * @brief Main command
 */
int main(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-c")) {
            // They want us to execute a command
            execvpe((const char*)argv[2], (const char**)&argv[2], NULL);
            return EXIT_FAILURE;
        }
    }
    
    printf("Essence v1.0\n");
    while (1) {
        printf("%s", essence_getPrompt());
        fflush(stdout);
        char *buf = essence_getInput();

        int argc;
        char **argv = essence_parse(buf, &argc);

        // Check error
        if (!argv) {
            printf("essence: error parsing %s\n", buf);
            free(buf);
            continue;
        }
        free(buf);

        // Execute command
        essence_executeCommand(argv[0], argc, argv);

        free(argv);
    }
    return 0;
}