/**
 * @file userspace/miniutils/readlink.c
 * @brief readlink
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "readlink: missing operand\n");
        fprintf(stderr, "Try \'readlink --help\' for more information.\n");
        return 1;
    }

    // TODO: flush this out
    char *p = argv[1];

    char buffer[256];
    ssize_t bytesread = readlink(p, buffer, 255);

    if (bytesread < 0) {
        return 1;
    }

    buffer[bytesread] = 0;

    printf("%s\n", buffer);
    return 0;
}