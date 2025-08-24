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
#include <errno.h>
#include <pwd.h>

/* Essence prompt */
static char __essence_prompt[512];

static char __current_hostname[256];
static char __current_user[256];


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
    // Get the current user
    struct passwd *pwd = getpwuid(geteuid());
    if (!pwd) {
        snprintf(__current_user, 256, "unknown");
    } else {
        snprintf(__current_user, 256, "%s", pwd->pw_name);
    }

    // Get the hostname
    // TODO
    snprintf(__current_hostname, 256, "ethereal");

    char *cwd = getcwd(NULL, 512);
    snprintf(__essence_prompt, 128, "\033[0;32m%s\033[0m@\033[0;32m%s\033[0m:\033[0;34m%s\033[0m$ ", __current_user, __current_hostname, cwd);
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

        essence_parsed_command_t *parse = essence_parseCommand(buf);
        if (!parse) continue;

        // Execute
        essence_execute(parse);

        // Cleanup
        essence_cleanupParsed(parse);
    }

    return essence_last_exit_status;
}

/**
 * @brief Main command
 */
int main(int argc, char *argv[]) {
    if (argc == 1) goto __noargs;

    struct option options[] = {
        { .flag = NULL, .name = "command", .has_arg = required_argument, .val = 'c'},
        { .flag = NULL, .name = "version", .has_arg = no_argument, .val = 'v'},
        { .flag = NULL, .name = "help", .has_arg = no_argument, .val = 'h'},
        { .flag = NULL, .name = NULL, .has_arg = no_argument, .val = 0 },
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


                // Parse and run
                essence_parsed_command_t *parse = essence_parseCommand(optarg);
                essence_execute(parse);
                essence_cleanupParsed(parse);
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


__noargs:
    printf("Essence v%d.%d.%d\n", ESSENCE_VERSION_MAJOR, ESSENCE_VERSION_MINOR, ESSENCE_VERSION_LOWER);
    while (1) {
        printf("%s", essence_getPrompt());
        fflush(stdout);

        // Get a line of input
        char *buf = essence_getInput();
        if (!strlen(buf)) continue;

        // Parse the input
        essence_parsed_command_t *parse = essence_parseCommand(buf);
        if (!parse) continue;

#ifdef ESSENCE_DEBUG_COMMAND_PARSER
        int i = 0;
        foreach(parse_node, parse->commands) {
            essence_command_t *cmd = (essence_command_t*)parse_node->value;
            printf("essence: cmd %d: ", i);

            for (int i = 0; i < cmd->argc; i++) {
                printf("%s ", cmd->argv[i]);
            }

            printf("\n\targc=%d\n\tfds: ", cmd->argc);
        
            if (cmd->redirs) {
                foreach(kn, cmd->redirs) {
                    essence_fd_redir_t *rd = (essence_fd_redir_t*)kn->value;
                    printf("%d = %d", rd->srcfd, rd->dstfd);
                }
            }

            printf("\n");
            i++;
        }
#endif

        // Execute the parsed input
        essence_execute(parse);

        // Cleanup
        // essence_cleanupParsed(parse);
    }
    return 0;
}