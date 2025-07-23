/**
 * @file userspace/essence/command.c
 * @brief Essence command executer and waiter
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
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

/* Prototype */
int essence_command(int argc, char *argv[]);

/* Command list */
builtin_command_t command_list[] = {
    ESSENCE_REGISTER_COMMAND("version", 1, NULL, essence_command),
    ESSENCE_REGISTER_COMMAND("cd", 1, NULL, cd),
    ESSENCE_REGISTER_COMMAND("env", 1, NULL, env),
    ESSENCE_REGISTER_COMMAND("export", 1, NULL, export),
    ESSENCE_REGISTER_COMMAND("exit", 1, NULL, exit_cmd),
    ESSENCE_REGISTER_COMMAND("unset", 2, NULL, unset)
};

/* Last exit status */
int essence_last_exit_status = 0;

/**
 * @brief Execute a built-in Essence command 
 * @param cmd The command to run
 * @param argc Argument count to the command
 * @param argv The argument pointer
 * @returns The exit status of the child process created
 */
int essence_executeBuiltinCommand(builtin_command_t cmd, int argc, char *argv[]) {
    // First validate that argc is enough
    if (argc < cmd.minimum_argc) {
        // Do usage
        cmd.usage();
        return 0;
    }

    // It is, now try to execute the command
    return cmd.cmd(argc, argv);
}


/**
 * @brief Wait for execution of a command to finish
 * @param cpid The child PID to wait on
 * @returns The exit status of the child
 */
int essence_waitForExecution(pid_t cpid) {
    int exit_code = -1;
    
    while (1) {
        int wstatus = 0;
        pid_t pid = waitpid(cpid, &wstatus, WSTOPPED);

        if (WIFSTOPPED(wstatus)) {
            printf("[Process %d stopped]\n", cpid);
        }

        if (pid == -1 && errno == ECHILD) {
            break;
        }

        if (pid == cpid) {
            exit_code = WEXITSTATUS(wstatus);
            break;
        }
    }

    return exit_code;
}

/**
 * @brief Execute a command in Essence
 * @param cmd The command to execute
 */
void essence_executeCommand(essence_command_t *cmd) {
    if (!cmd->argc) return;
    pid_t cpid = -1;

    // First, check if the command is a builtin
    for (size_t i = 0; i < sizeof(command_list) / sizeof(builtin_command_t); i++) {
        if (!strcmp(command_list[i].name, cmd->argv[0])) {
            // We have a builtin command, try executing that
            essence_last_exit_status = essence_executeBuiltinCommand(command_list[i], cmd->argc, cmd->argv);
            return;
        }
    }

    // Nope, fork off
    cpid = fork();
    if (!cpid) {
        // Prepare TTY
        setpgid(0, 0);
        tcsetpgrp(STDIN_FILENO, getpid());

        // Prepare file descriptors
        if (cmd->redirs) {
            foreach(rdnode, cmd->redirs) {
                essence_fd_redir_t *rd = (essence_fd_redir_t*)rdnode->value;
                dup2(rd->srcfd, rd->dstfd);
            }
        }

        // Duplications performed, now execute
        execvpe(cmd->argv[0], (const char **)cmd->argv, cmd->environ);

        // Execution failed.. what happened?
        if (errno == ENOENT) {
            // Not found
            printf("essence: %s: command not found\n", cmd->argv[0]);
            exit(127); 
        } else {
            printf("essence: %s: %s\n", cmd->argv[0], strerror(errno));
            exit(1);
        }
    }

    if (cpid < 0) return;

    // Wait on execution?
    if (!cmd->nowait) {
        essence_last_exit_status = essence_waitForExecution(cpid);
    } else {
        printf("essence: PID %d spawned in the background\n", cpid);
    }
}

/**
 * @brief Execute a parser frame
 * @param parse The parsed data returned by @c essence_parseCommand
 */
void essence_execute(essence_parsed_command_t *parse) {
    foreach(cmd_node, parse->commands) {
        essence_command_t *cmd = (essence_command_t*)cmd_node->value;
        essence_executeCommand(cmd);
    }
}