/**
 * @file userspace/miniutils/ethereal-version.c
 * @brief Get Ethereal version
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
#include <stdio.h>

int main(int argc, char *argv[]) {
    const ethereal_version_t *ver = ethereal_getVersion();

    printf("%s v%d.%d.%d (codename %s)\n", ver->name, ver->version_major, ver->version_minor, ver->version_lower, ver->codename);
    printf("Ethereal is licensed under the BSD-3 clause license.\n");

    return 0;
}