/**
 * @file userspace/celestial-theme/celestial-theme.c
 * @brief Userspace utility to set the Celestial default theme
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#include <ethereal/celestial/request.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc > 2) {
        fprintf(stderr, "Usage: celestial-theme [theme]\n");
        return 1;
    }

    if (argc == 2 && celestial_setServerTheme(argv[1]) < 0) {
        perror("celestial-theme");
        return 1;
    }

    char theme[CELESTIAL_THEME_NAME_MAX];
    if (celestial_getServerTheme(theme, sizeof(theme)) < 0) {
        perror("celestial-theme");
        return 1;
    }

    printf("%s\n", theme);
    return 0;
}
