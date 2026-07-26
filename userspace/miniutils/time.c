/**
 * @file userspace/miniutils/time.c
 * @brief Check times of process
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/times.h>
#include <sys/wait.h>
#include <sys/types.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s command [arguments ...]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    struct tms start_tms;
    clock_t start = times(&start_tms);
    if (start == (clock_t)-1) {
        perror("times");
        exit(EXIT_FAILURE);
    }

    pid_t cpid = fork();

    if (!cpid) {
        execvp(argv[1], &argv[1]);
        perror("execvp");
        exit(EXIT_FAILURE);
    }

    int status;
    if (waitpid(cpid, &status, 0) < 0) {
        perror("waitpid");
        exit(EXIT_FAILURE);
    }

    struct tms end_tms;
    clock_t end = times(&end_tms);
    if (end == (clock_t)-1) {
        perror("times");
        exit(EXIT_FAILURE);
    }


    double real = (double)(end - start) / 1000000000;
    double usr = (double)(end_tms.tms_cutime - start_tms.tms_cutime) / 1000000000;
    double sys = (double)(end_tms.tms_cstime - start_tms.tms_cstime) / 1000000000;

    printf("real %5dm%.3fs\n", (int)real / 60, (real - ((int)real/60)));
    printf("usr  %5dm%.3fs\n", (int)usr / 60, (usr - ((int)real/60)));
    printf("sys  %5dm%.3fs\n", (int)sys / 60, (sys - (int)(sys/60)));
    return 0;
}