/**
 * @file userspace/miniutils/touch.c
 * @brief touch
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("usage: touch <filename>\n");
        return 1;
    }

    long f = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC, 0644);

    if (f < 0) {
        printf("touch: %s: %s\n", argv[1], strerror(errno));
        return 1;
    }

    close(f);
    return 0;
}