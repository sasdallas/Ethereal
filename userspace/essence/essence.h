/**
 * @file userspace/essence/essence.h
 * @brief Essence header file
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef ESSENCE_H
#define ESSENCE_H

/**** INCLUDES ****/
#include <stdint.h>

/**** DEFINITIONS ****/

#define ESSENCE_VERSION_MAJOR       1
#define ESSENCE_VERSION_MINOR       0
#define ESSENCE_VERSION_LOWER       0

/**** TYPES ****/

typedef void (*usage_t)();

typedef int (*command_func_t)(int argc, char *argv[]);

typedef struct command {
    char name[64];
    int minimum_argc;
    usage_t usage;
    command_func_t cmd;
} command_t;

/**** MACROS ****/

#define ESSENCE_REGISTER_COMMAND(n, min, us, c) \
    (command_t){ .name = n, .minimum_argc = min, .usage = us, .cmd = c}

extern command_t command_list[];
extern int essence_last_exit_status;

/**** FUNCTIONS ****/

/**
 * @brief Parse a command into argc and argv
 * @param in_command The input command string to parse
 * @param out_argc Output for argc
 * @returns A pointer to argv or NULL if parsing failed
 */
char **essence_parse(char *in_command, int *out_argc);

/**
 * @brief Get a fully processed line of input
 */
char *essence_getInput();

/**
 * @brief Get the essence prompt
 */
char *essence_getPrompt();

/**
 * @brief Try to run a command and wait on it to execute
 * @param cmd The command to run (argv[0])
 * @param argc Argument count to the command
 * @param argv The argument pointer
 */
void essence_executeCommand(char *cmd, int argc, char *argv[]);

/**** COMMANDS ****/

int cd(int argc, char *argv[]);
int env(int argc, char *argv[]);
int export(int argc, char *argv[]);

#endif