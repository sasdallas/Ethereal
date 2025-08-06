/**
 * @file libkstructures/include/structs/ini.h
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

#ifndef STRUCTS_INI_H
#define STRUCTS_INI_H

/**** INCLUDES ****/
#include <stdint.h>
#include <structs/list.h>
#include <structs/hashmap.h>
#include <stdio.h>

/**** TYPES ****/

/**
 * @brief INI file
 */
typedef struct ini {
    hashmap_t *sections;
} ini_t;

/**** FUNCTIONS ****/

/**
 * @brief Load INI file
 * @param filename The file to load
 * @returns New INI object
 */
ini_t *ini_load(char *filename);

/**
 * @brief Get values from section
 * @param ini The INI file
 * @param section The section to get
 * @returns Hashmap of entries
 */
hashmap_t *ini_getSectionValues(ini_t *ini, char *section);

/**
 * @brief Get value from section
 * @param ini The INI file
 * @param section The section to get from
 * @param key The key to look for
 * @returns Value
 */
char *ini_get(ini_t *ini, char *section, char *key);

/**
 * @brief Destroy INI object
 * @param ini The INI object to destroy
 */
void ini_destroy(ini_t *ini);

#endif
