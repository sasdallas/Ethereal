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
#include <assert.h>
#include <fcntl.h>

/* Maximum supported argv */
#define MAX_ARGV        1024

/* Check to see if the buffer is valid */
#define ESSENCE_CHECK_BUFFER() { if (buffer_len >= buffer_size ) { buffer = realloc(buffer, buffer_size * 2); buffer_size *= 2; }}

/* Next character */
#define ESSENCE_NEXT_CHARACTER() { goto _loop_continue; }

/* Push and check buffer (and also push and go to next loop iter) */
#define ESSENCE_PUSH(ch) { buffer[buffer_len] = ch; buffer_len++; ESSENCE_CHECK_BUFFER(); }
#define ESSENCE_ONLY_PUSH(ch) { backslash = 0; ESSENCE_PUSH(ch); ESSENCE_NEXT_CHARACTER(); }

/* Pop last */
#define ESSENCE_POP(ch) ({ ch = '\0'; if (buffer_len > 1) { ch = buffer[buffer_len-1]; buffer_len--; }; })

/* Create a new argv */
#define ESSENCE_NEW_ARGV() { buffer[buffer_len] = 0; argv[argc] = buffer; argc++; buffer = malloc(128); buffer_len = 0; buffer_size = 128; } 

/**
 * @brief Parse a command into @c essence_parsed_command_t
 * @param in The input command string to parse
 * @returns Essence parsed command
 */
essence_parsed_command_t *essence_parseCommand(char *in) {
    essence_parsed_command_t *parse = malloc(sizeof(essence_parsed_command_t));
    parse->commands = list_create("essence parsed commands");

    char *cmd = in;             // Current command pointer

    while (1) {
        essence_command_t *cmdobj = malloc(sizeof(essence_command_t));
        memset(cmdobj, 0, sizeof(essence_command_t));


        // Let's start parsing this command
        char **argv = malloc(MAX_ARGV * sizeof(char*)); // !!!: mem waste
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
        essence_fd_redir_t *waiting_redir = NULL;

        char *p = cmd;

        // First, get rid of starting spaces
        while (*p == ' ') p++;

        // Now parse
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

                    // Update waiting redir if we need to
                    if (waiting_redir) {
                        goto _finished;
                    } else {
                        ESSENCE_NEW_ARGV();
                    }

                    ESSENCE_NEXT_CHARACTER();
                
                case '"':
                    if (backslash || quoted_single) ESSENCE_ONLY_PUSH(*p);
                    quoted = (quoted ? 0 : 1);
                    ESSENCE_NEXT_CHARACTER();
                    
                case '\\':
                    if (backslash || quoted_single) ESSENCE_ONLY_PUSH(*p);
                    backslash = 1;
                    ESSENCE_NEXT_CHARACTER();

                case '&':
                    if (backslash || quoted) ESSENCE_ONLY_PUSH(*p);

                    // Is the next character also an &?
                    p++;
                    if (*p == '&') {
                        // Yes, so we'll wait on these. Go next.
                        p++;
                        goto _finished;
                    } else {
                        // No, we won't wait on it.
                        cmdobj->nowait = 1;
                        goto _finished;
                    }

                case '>':
                    // Redirection character
                    if (backslash || quoted) ESSENCE_ONLY_PUSH(*p);
                    cmdobj->redirs = list_create("essence redirections");

                    // Pop last character
                    char a;
                    ESSENCE_POP(a);

                    // Get the redirection file number, if there is one
                    int redirect_fd = STDOUT_FILENO;
                    if (a && isdigit(a)) redirect_fd = (a - 'a');
                    else if (a) ESSENCE_PUSH(a);
                    
                    // We've pulled everything we need to, trigger a new argv
                    ESSENCE_NEW_ARGV();
                    argc--;

                    printf("essence: redirecting fd %d\n", redirect_fd);

                    // Create redir object
                    essence_fd_redir_t *rd = malloc(sizeof(essence_fd_redir_t));
                    rd->dstfd = redirect_fd;
                    rd->srcfd = -1;
                    list_append(cmdobj->redirs, rd);

                    // Next character
                    p++;
                    while (*p == ' ') p++;
                    
                    // Set redirection fd that we are awaiting
                    waiting_redir = rd;

                    p--; // For essence next character
                    ESSENCE_NEXT_CHARACTER();

                case '#':
                    if (!buffer_len) {
                        if (quoted_single || !quoted) {
                            // Ignore everything else from here
                            // Consume this argv
                            free(buffer);

                            p = p + strlen(p);
                            goto _really_finished;
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

        if (quoted) {
            // TODO: Either implement a solution or not leak memory
            fprintf(stderr, "essence: Unterminated quoted argument and support for continuing quotes isn't implemented\n");
            return NULL;
        }

        buffer[buffer_len] = 0; // Null-terminate buffer

        // We might be finished, unless we have a waiting redirection (>)
        if (waiting_redir) {
            if (!buffer_len) {
                fprintf(stderr, "essence: Token \'>\' expects an argument.\n");
                return NULL; // TODO: Cleanup
            }

            // We have a waiting redirection
            printf("essence: opening %s\n", buffer);
            
            // Depending on whether the buffer begins with & they might want us to redirect an fd
            if (*buffer == '&') {
                // Redirect file descriptor
                char *b = buffer+1;
                waiting_redir->srcfd = strtol((const char*)b, NULL, 10);
            } else {
                // Create the file instead
                waiting_redir->srcfd = open(buffer, O_CREAT | O_RDWR);
                if (waiting_redir->srcfd < 0) {
                    perror("open");
                    return parse;
                }
            }

            // Cleanup by revoking buffer
            free(buffer);
            buffer_len = 0;
        } else if (buffer_len) {
            // Push final argument
            argv[argc] = buffer;
            argc++;
        }


_really_finished:
        // Clean up
        argv[argc] = NULL;

        // Now add the command object
        cmdobj->argv = argv;
        cmdobj->argc = argc;
        cmdobj->environ = environ;
        list_append(parse->commands, cmdobj);

        // Now set cmd
        cmd = p;
        if (!(*cmd)) break;
    }

    return parse;
}

/**
 * @brief Cleanup the parsed data
 * @param parse The parsed data that you received
 */
void essence_cleanupParsed(essence_parsed_command_t *parse) {
    foreach(pn, parse->commands) {
        essence_command_t *cmd = (essence_command_t*)pn->value;
        
        // Free argv
        for (int i = 0; i < cmd->argc; i++) free(cmd->argv[i]);
        free(cmd->argv);

        // Free redirections
        if (cmd->redirs) list_destroy(cmd->redirs, true);
    }

    list_destroy(parse->commands, true);
    free(parse);
}