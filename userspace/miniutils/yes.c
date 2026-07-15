/**
 * @file userspace/miniutils/yes.c
 * @brief yes command
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

int main(int argc, char *argv[]) {
    char *string = "y";
    if (argc > 1) {
        string = argv[1];
    }

    for (;;) {
        printf("%s\n", string);
    }

    return 0;
}
