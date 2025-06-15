/**
 * @file userspace/miniutils/clear.c
 * @brief clear program, which uses the ANSI escape sequence \033[2J
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

int main(int argc, char *argv[]) {
    // TODO: Make this part of the terminal?
    printf("\033[2J");
    fflush(stdout);
    return 0;
}