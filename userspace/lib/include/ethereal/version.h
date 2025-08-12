/**
 * @file userspace/lib/include/ethereal/version.h
 * @brief Ethereal version API (/etc/ethereal-version)
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _VERSION_H
#define _VERSION_H


/**** TYPES ****/

typedef struct ethereal_version {
    char *name;
    char *codename;
    int version_major;
    int version_minor;
    int version_lower;
} ethereal_version_t;

/**** FUNCTIONS ****/

/**
 * @brief Retrieve Ethereal version
 * @returns NULL on failure
 */
const ethereal_version_t *ethereal_getVersion();

#endif