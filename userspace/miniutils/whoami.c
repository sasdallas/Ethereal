/**
 * @file userspace/miniutils/whoami.c
 * @brief whoami
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
#include <pwd.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    struct passwd *pw = getpwuid(geteuid());

    if (pw) {
        printf("%s\n", pw->pw_name);
    } else {
        printf("unknown\n");
    }

    return 0;
}