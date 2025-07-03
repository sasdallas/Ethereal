/**
 * @file userspace/essence/essence.h
 * @brief Essence header file
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef ESSENCE_H
#define ESSENCE_H

/**** INCLUDES ****/
#include <stdint.h>
#include <structs/list.h>
#include <structs/hashmap.h>

/**** DEFINITIONS ****/

#define ESSENCE_VERSION_MAJOR       1
#define ESSENCE_VERSION_MINOR       2
#define ESSENCE_VERSION_LOWER       0

/**** TYPES ****/

typedef void (*usage_t)();

typedef int (*builtin_command_func_t)(int argc, char *argv[]);

typedef struct builtin_command {
    char name[64];
    int minimum_argc;
    usage_t usage;
    builtin_command_func_t cmd;
} builtin_command_t;

typedef struct essence_fd_redir {
    int srcfd;
    int dstfd;
} essence_fd_redir_t;

typedef struct essence_command {
    int argc;               // argc
    char **argv;            // argv
    char **environ;         // environ
    list_t *redirs;         // Redirections (source fd -> destination fd)
    int nowait;             // Do not wait on this command
} essence_command_t;

typedef struct essence_parsed_command {
    list_t *commands;       // Command list
} essence_parsed_command_t;

/**** MACROS ****/

#define ESSENCE_REGISTER_COMMAND(n, min, us, c) \
    (builtin_command_t){ .name = n, .minimum_argc = min, .usage = us, .cmd = c}

extern builtin_command_t command_list[];
extern int essence_last_exit_status;

/**** FUNCTIONS ****/

/**
 * @brief Parse a command into argc and argv
 * @param in_command The input command string to parse
 * @param out_argc Output for argc
 * @param out_argv Output for argv
 * @returns 0 on success
 */
int essence_parse(char *in_command, int *out_argc, char ***out_argv);

/**
 * @brief Get a fully processed line of input
 */
char *essence_getInput();

/**
 * @brief Get the essence prompt
 */
char *essence_getPrompt();


/**
 * @brief Execute a command in Essence
 * @param cmd The command to execute
 */
void essence_executeCommand(essence_command_t *cmd);

/**
 * @brief Parse a command into @c essence_parsed_command_t
 * @param cmd The input command string to parse
 * @returns Essence parsed command
 */
essence_parsed_command_t *essence_parseCommand(char *cmd);

/**
 * @brief Cleanup the parsed data
 * @param parse The parsed data that you received
 */
void essence_cleanupParsed(essence_parsed_command_t *cmd);

/**
 * @brief Execute a parser frame
 * @param parse The parsed data returned by @c essence_parseCommand
 */
void essence_execute(essence_parsed_command_t *parse);

/**** COMMANDS ****/

int cd(int argc, char *argv[]);
int env(int argc, char *argv[]);
int export(int argc, char *argv[]);
int exit_cmd(int argc, char *argv[]);
int unset(int argc, char *argv[]);

#endif