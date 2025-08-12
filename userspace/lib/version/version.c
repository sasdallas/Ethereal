/**
 * @file userspace/lib/version/version.c
 * @brief Stupid /etc/ethereal-version parser
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <ethereal/version.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* Last retrieved Ethereal version */
static ethereal_version_t __version = { 0 };

/**
 * @brief Retrieve Ethereal version
 * @returns NULL on failure
 */
const ethereal_version_t *ethereal_getVersion() {
    if (__version.name) return (const ethereal_version_t *)&__version;

    FILE *f = fopen("/etc/ethereal-version", "r");
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buffer = malloc(fsize);
    if (fread(buffer, fsize, 1, f) != 1) {
        free(buffer);
        return NULL;
    }

    char *save;
    char *pch = strtok_r(buffer, "\n", &save);

    while (pch) {
        // Create key/value pair
        char *key = pch;
        char *value = strchr(key, '=');
        if (!value) continue;

        *value = 0;
        value++;

        if (!strcmp(key, "NAME")) {
            __version.name = strdup(value);
        } else if (!strcmp(key, "CODENAME")) {
            __version.codename = strdup(value);
        } else if (!strcmp(key, "VERSION_MAJOR")) {
            __version.version_major = strtol(value, NULL, 10);
        } else if (!strcmp(key, "VERSION_MINOR")) {
            __version.version_minor = strtol(value, NULL, 10);
        } else if (!strcmp(key, "VERSION_LOWER")) {
            __version.version_lower = strtol(value, NULL, 10);
        }

        pch = strtok_r(NULL, "\n", &save);
    }

    free(buffer);
    fclose(f);
     
    return (const ethereal_version_t*)&__version;
}