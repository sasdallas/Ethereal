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
#include <stdio.h>

/* Prototype */
int essence_command(int argc, char *argv[]);

/* Command list */
command_t command_list[] = {
    ESSENCE_REGISTER_COMMAND("version", 1, NULL, essence_command),
    ESSENCE_REGISTER_COMMAND("cd", 1, NULL, cd),
    ESSENCE_REGISTER_COMMAND("env", 1, NULL, env),
    ESSENCE_REGISTER_COMMAND("export", 1, NULL, export),
    ESSENCE_REGISTER_COMMAND("exit", 1, NULL, exit_cmd),
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
int essence_executeBuiltinCommand(command_t cmd, int argc, char *argv[]) {
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
        pid_t pid = waitpid(cpid, &wstatus, 0);

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
 * @brief Try to run a command and wait on it to execute
 * @param cmd The command to run (argv[0])
 * @param argc Argument count to the command
 * @param argv The argument pointer
 */
void essence_executeCommand(char *cmd, int argc, char *argv[]) {
    if (!cmd || !(*cmd)) return;

    pid_t cpid = -1;
    int wait_for_child = !(argc > 1 && !strcmp(argv[argc-1], "&"));

    // First, check if the command is a builtin
    for (size_t i = 0; i < sizeof(command_list) / sizeof(command_t); i++) {
        if (!strcmp(command_list[i].name, cmd)) {
            // We have a builtin command, try executing that
            essence_last_exit_status = essence_executeBuiltinCommand(command_list[i], argc, argv);
            return;
        }
    }

    // It's not a builtin, try to fork and execute it
    cpid = fork();
    if (!cpid) {
        // We are the child process, so try to run.
        execvpe(cmd, (const char**)argv, environ);

        // Execution failed.. what happened?
        if (errno == ENOENT) {
            // Not found
            printf("essence: %s: command not found\n", cmd);
            exit(127); 
        } else {
            printf("essence: %s: %s\n", cmd, strerror(errno));
            exit(1);
        }
    }

    if (cpid < 0) return;


    // Wait on execution?
    if (wait_for_child) {
        essence_last_exit_status = essence_waitForExecution(cpid);
    } else {
        printf("essence: PID %d spawned in the background\n", cpid);
    }
}
