/**
 * @file userspace/miniutils/net-startup.c
 * @brief Networking startup
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
#include <dirent.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    DIR *d = opendir("/device");
    if (!d) {
        perror("opendir");
        return 1;
    }

    if (chdir("/device/") < 0) {
        perror("chdir");
        return 1;
    }

    struct dirent *ent;
    while ((ent = readdir(d))) {
        if (strstr(ent->d_name, "enp") == ent->d_name) {
            // try running dhcpcli on them
            printf("Configuring network interface \"%s\"...\n", ent->d_name);
            char cmd[256];
            snprintf(cmd, 256, "dhcpcli %s", ent->d_name);
            int r = system(cmd);
            if (r != 0) {
                printf("dhcpcli failed with error code %d\n", r);
            }
        }
    }

    closedir(d);
    return 0;
}
