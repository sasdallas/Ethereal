/**
 * @file userspace/miniutils/drivers.c
 * @brief Get all drivers currently loaded in memory
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h> 
#include <stdlib.h>

/* Temporary */
char tmp[512];

void print_process_details(struct dirent *ent) {
    snprintf(tmp, 512, "/kernel/drivers/%s/info", ent->d_name);

    // Get the file
    FILE *f = fopen(tmp, "r");
    if (!f) {
        printf("drivers: %s: %s\n", tmp, strerror(errno));
        return;
    }

    // Get file length
    long length;
    fseek(f, 0, SEEK_END);
    length = ftell(f);
    fseek(f, 0, SEEK_SET);

    // Read the file in
    char *buffer = malloc(length);
    memset(buffer, 0, length);
    fread(buffer, length, 1, f);

    // Time to do some stringing
    char *save;
    char *pch = strtok_r(buffer, "\n", &save);
    // !!!: Consistently, in this codebase, I find the weirdest ways possible to do strings. What am I doing?
    int i = 0;
    while (pch) {
        // Find the colon
        char *colon = strchr(pch, ':');
        if (!colon) break; // No colon?
        colon++;

        // Calculate space
        if (strlen(colon) < 24) {
            for (unsigned int i = 0; i < 24 - strlen(colon); i++) {
                tmp[i] = ' ';
                tmp[i+1] = 0;
            }
        }

        printf("%s%s", colon, tmp);
        
        
        i++;
        if (i >= 4) break;
        pch = strtok_r(NULL, "\n", &save);
    }

    putchar('\n');
}

int main(int argc, char *argv[]) {
    DIR *dirp = opendir("/kernel/drivers");

    printf("FILENAME\t\t\t\tNAME\t\t\t\t\tAUTHOR\t\t\t\t\tLOAD LOCATION\n");

    struct dirent *ent = readdir(dirp);
    int i = 0;
    while (ent) {
        // Don't waste resources on parsing "." and ".."...
        if (i >= 2) print_process_details(ent);
        ent = readdir(dirp);
        i++;
    }

    closedir(dirp);
    return 0;
}