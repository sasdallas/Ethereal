/**
 * @file userspace/miniutils/users.c
 * @brief Lists users in the system
 * 
 * @note This lists actual users
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

int main(int argc, char *argv[]) {
    struct passwd *p = getpwent();

    while (p) {
        printf("%s\n", p->pw_name);
        p = getpwent();
    }

    return 0;
}