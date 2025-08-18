/**
 * @file userspace/miniutils/reboot.c
 * @brief Reboot utility
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <ethereal/reboot.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

int main(int argc, char *argv[]) {
    ethereal_reboot(REBOOT_TYPE_DEFAULT);
    fprintf(stderr, "poweroff: Failed to power off: %s\n", strerror(errno));
    return 1;
}