/**
 * @file userspace/miniutils/basename.c
 * @brief basename program
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
#include <string.h>
#include <libgen.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "basename: missing operand.\n");
        return 1;
    }

    printf("%s\n", basename(argv[1]));
    return 0;
}
