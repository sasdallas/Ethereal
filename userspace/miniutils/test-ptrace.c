/**
 * @file userspace/miniutils/test-ptrace.c
 * @brief ptrace test
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <stdio.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: test-ptrace [PROGRAM]\n");
        return 1;
    }

    pid_t cpid = fork();

    if (!cpid) {
        ptrace(PTRACE_TRACEME, 0, NULL, NULL);
        execvp(argv[1], (const char**)&argv[1]);
        return 1;
    } else {
        int status;
        pid_t res = waitpid(cpid, &status, WSTOPPED);
        
        // We got a response
        if (res >= 0) {
            if (WIFSTOPPED(status)) {
                
                if (WSTOPSIG(status) == SIGSTOP) {
                    printf("Process was stopped due to signal %d\n", WSTOPSIG(status));
                    printf("We have attached to process %d\n", cpid);
                
                    ptrace(PTRACE_SETOPTIONS, cpid, NULL, PTRACE_O_EXITKILL);
                    ptrace(PTRACE_SYSCALL, cpid, NULL, NULL);
                    waitpid(cpid, &status, WSTOPPED);

                    printf("The process has attempted to do a system call\n");
                    ptrace(PTRACE_CONT, cpid, NULL, NULL);
                }

                
            } else {
                printf("ERROR: Process exited due to unknown reason :(\n");
            }
        } else {
            perror("waitpid");
        }
    }


    return 0;
}