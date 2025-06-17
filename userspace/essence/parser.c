/**
 * @file userspace/essence/parser.c
 * @brief Parser for commands. Can handle environmental variables.
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include "essence.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <stdio.h>

/* Maximum supported argv */
#define MAX_ARGV        1024

/* Check to see if the buffer is valid */
#define ESSENCE_CHECK_BUFFER() { if (buffer_len >= buffer_size ) { buffer = realloc(buffer, buffer_size * 2); buffer_size *= 2; }}

/* Next character */
#define ESSENCE_NEXT_CHARACTER() { goto _loop_continue; }

/* Push and check buffer (and also push and go to next loop iter) */
#define ESSENCE_PUSH(ch) { buffer[buffer_len] = ch; buffer_len++; ESSENCE_CHECK_BUFFER(); }
#define ESSENCE_ONLY_PUSH(ch) { backslash = 0; ESSENCE_PUSH(ch); ESSENCE_NEXT_CHARACTER(); }

/* Create a new argv */
#define ESSENCE_NEW_ARGV() { buffer[buffer_len] = 0; argv[argc] = buffer; argc++; buffer = malloc(128); buffer_len = 0; buffer_size = 128; } 

/**
 * @brief Parse a command into argc and argv
 * @param in_command The input command string to parse
 * @param out_argc Output for argc
 * @param out_argv Output for argv
 * @returns 0 on success
 */
int essence_parse(char *in_command, int *out_argc, char ***out_argv) {
    // Let's start parsing this command
    char **argv = malloc(MAX_ARGV * sizeof(char*));
    assert(argv);
    int argc = 0;

    // VARIABLES WHICH CONTROL ESSENCE BEHAVIOR
    int quoted = 0;                 // Whether the string is under quotations. Essence will usually treat these literally
    int quoted_single = 0;          // Single quotations ('') signal to ignore almost every special character
    int backslash = 0;              // Whether the character is under backslash.

    char *buffer = malloc(128);     // Default argv buffer size
    size_t buffer_len = 0;
    size_t buffer_size = 128;

    // We're going to parse the ENTIRE command at once.
    char *p = in_command;
    while (*p) {
        switch (*p) {
            case '$':
                // We have a potential special character - do we parse?
                if (backslash || quoted_single) ESSENCE_ONLY_PUSH(*p); 
                if (*(p+1) == ' ' || !(*(p+1))) ESSENCE_ONLY_PUSH(*p);

                // Yeah, we have to start parsing this character.
                // Check for special syntax:
                // $$ is for the shell PID
                // $? is for the last command exit status
                // $# is for the shell argc
                // $RANDOM is for a random varialbe
                // Else, we get a variable

                char tmp[128] = { 0 };
                int len = 0;
                p++;
                if (*p == '$') {
                    // Shell PID
                    snprintf(tmp, 128, "%d", getpid());
                    len++;
                } else if (*p == '?') {
                    // Last exit status
                    snprintf(tmp, 128, "%d", essence_last_exit_status);
                    len++;
                } else if (*p == '#') {
                    // TODO
                    snprintf(tmp, 128, "0");
                    len++;
                } else {
                    // Put a NULL character where there's a space so we can get the variable
                    char *p2 = p;
                    char saved = 0;
                    while (*p2) {
                        if (*p2 == ' ' || !isalnum(*p2)) {
                            saved = *p2;
                            *p2 = 0;
                            break;
                        }
                        len++;
                        p2++;
                    }

                    // If the variable is RANDOM, get a random number
                    if (!strcmp(p, "RANDOM")) {
                        snprintf(tmp, 128, "%d", rand() % RAND_MAX);
                    } else {
                        // Get environ
                        char *env = getenv(p);
                        if (!env) {
                            p += len;
                            ESSENCE_NEXT_CHARACTER();
                        }

                        snprintf(tmp, 128, "%s", env); // !!!: What if environment variable size greater?
                    }

                    // Restore p2
                    *p2 = saved;
                }

                // Place tmp as an argument
                for (size_t i = 0; i < strlen(tmp); i++) ESSENCE_PUSH(tmp[i]);
                p += len;

                ESSENCE_NEXT_CHARACTER();

            case ' ':
                if (quoted) ESSENCE_ONLY_PUSH(*p);
                ESSENCE_NEW_ARGV();
                ESSENCE_NEXT_CHARACTER();
            
            case '"':
                if (backslash || quoted_single) ESSENCE_ONLY_PUSH(*p);
                quoted = (quoted ? 0 : 1);
                ESSENCE_NEXT_CHARACTER();
                break;

            case '#':
                if (!buffer_len) {
                    if (quoted_single || !quoted) {
                        // Ignore everything else from here
                        // Consume this argv
                        free(buffer);
                        goto _finished;
                    }
                }

                ESSENCE_ONLY_PUSH(*p);
                ESSENCE_NEXT_CHARACTER();

            default:
                ESSENCE_ONLY_PUSH(*p);
        }

    _loop_continue:
        p++;
        continue;

    _finished:
        break;
    }

    // Push final argument
    if (buffer_len) {
        buffer[buffer_len] = 0;
        argv[argc] = buffer;
        argc++;
    }

    // Clean up
    argv[argc] = NULL;
    *out_argc = argc;
    *out_argv = argv;
    return 0;
}