/**
 * @file userspace/miniutils/trace-proc.c
 * @brief trace a process
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/signal.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: trace-proc <program> [args...]\n");
        return 1;
    }

    SYSCALL0(999);

    char *child_argv[argc];
    for (int i = 1; i < argc; i++) {
        child_argv[i - 1] = argv[i];
    }
    child_argv[argc - 1] = NULL;

    execvp(argv[1], child_argv);

    return 0;
}
