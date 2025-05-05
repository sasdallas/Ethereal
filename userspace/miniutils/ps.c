/**
 * @file userspace/miniutils/ps.c
 * @brief ps
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

char buffer[128];

char *get_process_name(char *proc_dir_name) {
    char name[512];
    snprintf(name, 512, "/kernel/processes/%s/info", proc_dir_name);

    FILE *f = fopen(name, "r");
    memset(buffer, 0, 128);
    fread(buffer, 128, 1, f);
    buffer[strlen(buffer)-1] = 0;
    fclose(f);

    char *procname = strrchr(buffer, ':');
    procname++;
    return procname;
}


int main(int argc, char *argv[]) {
    DIR *dirp = opendir("/kernel/processes");

    printf("PID\t\t\tNAME\n");

    struct dirent *ent = readdir(dirp);
    while (ent) {
        if (isdigit(ent->d_name[0])) {
            // Get process name
            printf("%s  \t\t\t%s\n", ent->d_name, get_process_name(ent->d_name));
        }

        ent = readdir(dirp);

    }

    // closedir(dirp);
    return 0;
}