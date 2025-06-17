/**
 * @file userspace/essence/essence.c
 * @brief Essence shell
 * 
 * Essence is a bash-like shell designed for Ethereal
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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>

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
 * @brief Essence usage
 */
void usage() {
    printf("Usage: essence [-h] [-v] [-c COMMAND] [SCRIPT]\n");
    printf("Bash like shell for Ethereal\n\n");
    printf(" -h, --help         Display this help message\n");
    printf(" -v, --version      Print the version of Essence\n");
    printf(" -c, --command      Execute command\n");
    exit(EXIT_FAILURE);
}

/**
 * @brief Essence version
 */
void version() {
    printf("Essence v%d.%d.%d\n", ESSENCE_VERSION_MAJOR, ESSENCE_VERSION_MINOR, ESSENCE_VERSION_LOWER);
    exit(EXIT_SUCCESS);
}

/**
 * @brief Execute a script for Essence
 * @param f The script file
 * @returns Exit code
 */
int essence_executeScript(FILE *f) {
    while (!feof(f)) {
        char buf[512] = { 0 };
        fgets(buf, 512, f);

        // Clear newline
        *(strchrnul(buf, '\n')) = 0;

        // Parse
        int cmd_argc;
        char **cmd_argv;
        int r = essence_parse(buf, &cmd_argc, &cmd_argv);

        // Check error
        if (r) {
            printf("essence: error parsing %s\n", buf);
            continue;
        }

        if (!cmd_argc) continue;
    
        // Execute command
        essence_executeCommand(cmd_argv[0], cmd_argc, cmd_argv);

        free(cmd_argv);
    }

    return essence_last_exit_status;
}

/**
 * @brief Main command
 */
int main(int argc, char *argv[]) {

    struct option options[] = {
        { .flag = NULL, .name = "--command", .has_arg = required_argument, .val = 'c'},
        { .flag = NULL, .name = "--version", .has_arg = no_argument, .val = 'v'},
        { .flag = NULL, .name = "--help", .has_arg = no_argument, .val = 'h'}
    };

    int c;
    int index ;
    while ((c = getopt_long(argc, argv, "c:vh", (const struct option*)&options, &index)) != -1) {
        if (!c && options[index].flag == 0) c = options[index].val;

        switch (c) {
            case 'c':
                // Is this the last one?
                if (!optarg) {
                    fprintf(stderr, "essence: option \'-c\' requires an argument\n");
                    return 1;
                }

                // They want us to execute a command
                int new_argc;
                char **new_argv;
                
                int r = essence_parse(optarg, &new_argc, &new_argv);
                if (r) {
                    printf("essence: error parsing %s\n", optarg);
                    return 125;
                }

                if (!new_argc) return 0;

                essence_executeCommand(new_argv[0], new_argc, new_argv);
                return essence_last_exit_status;

            case 'v':
                version();
                break;

            case 'h':
            default:
                usage();
                break;
        }
    }

    // Still more arguments to parse?
    if (optind < argc) {
        // Run this as a script
        FILE *f = fopen(argv[optind], "r");
        if (!f) {
            fprintf(stderr, "essence: %s: %s\n", argv[optind], strerror(errno));
            return 1;
        }

        return essence_executeScript(f);
    }

    printf("Essence v1.0\n");
    while (1) {
        printf("%s", essence_getPrompt());
        fflush(stdout);
        char *buf = essence_getInput();

        int cmd_argc;
        char **cmd_argv;
        int r = essence_parse(buf, &cmd_argc, &cmd_argv);

        // Check error
        if (r) {
            printf("essence: error parsing %s\n", buf);
            free(buf);
            continue;
        }
        
        free(buf);

        // Execute command
        essence_executeCommand(cmd_argv[0], cmd_argc, cmd_argv);

        free(cmd_argv);
    }
    return 0;
}