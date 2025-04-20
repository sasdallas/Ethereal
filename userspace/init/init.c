/**
 * @file userspace/init/init.c
 * @brief Basic shell, garbage 
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of reduceOS.
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

#define DEFAULT_BUFSIZE 128

#define CSR_SHOW() { putchar('\030'); fflush(stdout); }
#define CSR_HIDE() { printf("\b"); fflush(stdout); }

#define PUTCHAR_FLUSH(c) { putchar(c); fflush(stdout); }


/**
 * @brief Get a buffer
 * @returns A null-terminated buffer
 */
char *shell_readBuffer() {
    size_t bufsz = DEFAULT_BUFSIZE;
    char *buffer = malloc(bufsz);
    memset(buffer, 0, bufsz);
    int i = 0;  
    int c;

    while (1) {
        CSR_SHOW();
        c = getchar();

        switch (c) {
            case '\b':
                // Backspace character
                CSR_HIDE();
                if (i > 0) {
                    // Have space, go ahead
                    i--;
                    printf("\b");
                    fflush(stdout);
                }
                break;

            case '\n':
                // Newline, flush
                buffer[i] = 0;
                CSR_HIDE();
                PUTCHAR_FLUSH('\n');
                return buffer;

            default:
                putchar('\b');
                PUTCHAR_FLUSH(c);
                

                // Write char to buffer
                buffer[i] = c;
                i++;
                if ((size_t)i >= bufsz) {
                    bufsz = bufsz + DEFAULT_BUFSIZE;
                    buffer = realloc(buffer, bufsz);
                }
                break;
        }
    }

    // Unreachable
    return NULL;
}

/**
 * @brief Process command into argc, argv
 */
char **shell_processCommand(char *command, int *argc) {
    char **tokens = malloc(strlen(command) * sizeof(char*));
    memset(tokens, 0, strlen(command) * sizeof(char*));
    int i = 0;


    char *pch;
    char *save;

    pch = strtok_r(command, " ", &save);
    while (pch) {
        tokens[i] = pch;
        i++;

        // lol no reallocation
        pch = strtok_r(NULL, " ", &save);
    }

    tokens[i] = NULL;
    *argc = i; 
    return tokens;
}

/**
 * @brief Execute builtin command
 */
int shell_executeBuiltin(int argc, char *argv[]) {
    if (!strcmp(argv[0], "cd") && argc >= 2) {
        // CD command
        int e = chdir(argv[1]);
        if (e < 0) printf("Could not switch to directory \"%s\": errno %d\n", argv[1], errno);
        return 1;
    }

    return 0;
}

/**
 * @brief Main shell code
 */
void shell() {
    while (1) {
        char *cwd = getcwd(NULL, 512);

        printf("%s> ", cwd);
        fflush(stdout);

        char *buffer = shell_readBuffer();
        if (strlen(buffer) == 0) {
            free(buffer);
            continue;
        }

        int argc = 0;
        char **argv = shell_processCommand(buffer, &argc);

        // Check if builtin
        if (shell_executeBuiltin(argc, argv)) goto _next_cmd;

        pid_t child = fork();
        if (!child) {
            execvpe((const char*)argv[0], (const char**)argv, environ);
            printf("Error executing program \"%s\": %s\n", argv[0], strerror(errno));
            exit(1);
        } else {
            while (1) {
                int wstatus = 0;
                pid_t pid = waitpid(-1, &wstatus, 0);

                if (pid == -1 && errno == ECHILD) {
                    break;
                }

                if (pid == child) {
                    // printf("Process finished with exitcode %d\n", WEXITSTATUS(wstatus));
                    break;
                }
            }
        }

    _next_cmd:
 
        free(argv);
        free(buffer);
    }
}

int main(int argc, char *argv[]) {
    open("/device/stdin", O_RDONLY);
    open("/device/kconsole", O_RDWR);
    open("/device/kconsole", O_RDWR);

    putenv("PATH=/device/initrd/bin/:"); // TEMP

    printf("Welcome to Ethereal\n");
    printf("Initializing shell...\n");
    shell();
    return 0;
}
