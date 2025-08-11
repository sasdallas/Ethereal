/**
 * @file libkstructures/ini/ini.c
 * @brief INI file parsing
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <structs/ini.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef __LIBKSTRUCTURES
#include <kernel/fs/vfs.h>
#endif

/**
 * @brief Load INI file
 * @param filename The file to load
 * @returns New INI object
 */
ini_t *ini_load(char *filename) {
#ifdef __LIBKSTRUCTURES
    fs_node_t *node = kopen(filename, 0);
    if (!node) return NULL;

    char *buffer = malloc(node->length);
    memset(buffer, 0, node->length);
    if (fs_read(node, 0, node->length, (uint8_t*)buffer) != (ssize_t)node->length) {
        free(buffer);
        return NULL;
    }

    fs_close(node);
#else
    FILE *f = fopen(filename, "r");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buffer = malloc(fsize);
    if (fread(buffer, fsize, 1, f) != 1) {
        fclose(f);
        free(buffer);
        return NULL;
    }

    fclose(f);
#endif

    // Now we have a buffer, allocate an INI object
    ini_t *ini = malloc(sizeof(ini_t));
    ini->sections = hashmap_create("ini sections", 20);

    // For each line in the file
    char *save;
    char *pch = strtok_r(buffer, "\n", &save);

    hashmap_t *current_section = NULL;

    while (pch) {
        if (*pch == '[') {
            // Start of an INI file section
            pch++;
            *(strchrnul(pch, ']')) = 0;

            // Now pch contains the scetion name
            current_section = hashmap_create("ini file section", 10);
            hashmap_set(ini->sections, pch, current_section);
        } else if (*pch == ';') {
            goto _next_token; // Comment
        } else {
            // Must be a line
            if (!current_section) goto _next_token;

            // Separate into key value pair
            char *value = strchr(pch, '=');
            if (!value) value = strchr(pch, ':');
            if (!value) goto _next_token;

            *value = 0;
            value++;

            // While we have space, skip past it
            char *key = pch;
            while (*key == ' ') key++;
            if (!key) goto _next_token;
            *(strchrnul(key, ' ')) = 0;

            // Handle value
            char *real_value = value;
            while (*value == ' ') value++;
            if (*value == '"') {
                // We have to parse the string
                value++;
                real_value = value;
                while (*value && *value != '"') value++;
                if (!(*value)) goto _next_token;
                *value = 0;
            }

            // After all of that confusing string stuff, key and real_value are our pair
            hashmap_set(current_section, key, real_value);
        }


    _next_token:
        pch = strtok_r(NULL, "\n", &save);
    }

    return ini;

}

/**
 * @brief Get values from section
 * @param ini The INI file
 * @param section The section to get
 * @returns Hashmap of entries
 */
hashmap_t *ini_getSectionValues(ini_t *ini, char *section) {
    return hashmap_get(ini->sections, section);
}

/**
 * @brief Get value from section
 * @param ini The INI file
 * @param section The section to get from
 * @param key The key to look for
 * @returns Value
 */
char *ini_get(ini_t *ini, char *section, char *key) {
    hashmap_t *map = ini_getSectionValues(ini, section);
    if (!map) return NULL;
    return hashmap_get(map, key);
}

/**
 * @brief Destroy INI object
 * @param ini The INI object to destroy
 */
void ini_destroy(ini_t *ini) {
    if (!ini) return;
    if (ini->sections) hashmap_free(ini->sections);
    free(ini);
}