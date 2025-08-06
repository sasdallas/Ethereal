/**
 * @file userspace/miniutils/poweroff-dialog.c
 * @brief Power off with dialog
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>


int main(int argc, char *argv[]) {
    int y = system("/usr/bin/show-dialog --question --icon=/usr/share/icons/24/power.bmp --title=\"Power off\" --text=\"Do you want to power off?\"");

    if (y) {
        return 0;
    }


    system("poweroff");
    return 0;
}