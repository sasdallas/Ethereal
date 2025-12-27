/**
 * @file hexahedron/misc/args.c
 * @brief Kernel arguments handler
 * 
 * This handles the kernel arguments in a hashmap organization. It parses
 * the string given to @c kargs_init
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <kernel/misc/args.h>
#include <kernel/mm/alloc.h>
#include <kernel/debug.h>
#include <structs/hashmap.h>
#include <string.h>

/* Arguments hashmap */
hashmap_t *kargs = NULL;

/**
 * @brief Initialize the argument parser
 * @param args A string pointing to the arguments
 */
void kargs_init(char *args) {
    if (!args) return; // No kernel arguments provided

    args = strdup(args);
    kargs = hashmap_create("kernel argument map", 5);

    // The only thing about arguments is that they are separated by spaces
    // Arguments can also appear with equal signs, such as "--debug=console"

    char *pch = strtok(args, " ");
    while (pch) {
        // If we have a value process it
        char *n  = strchr(pch, '=');
        if (n) { *n = 0; n++; }

        hashmap_set(kargs, pch, n);

        pch = strtok(NULL, " ");
    }
}

/**
 * @brief Get the argument value for a key
 * @param arg The argument to get the value for
 * @returns NULL on not found or the value
 */
char *kargs_get(char *arg) {
    if (!kargs) return NULL;
    return hashmap_get(kargs, arg);
}

/**
 * @brief Returns whether the arguments list has an argument
 * @param arg The argument to check for
 */
int kargs_has(char *arg) {
    if (!kargs) return 0;
    return hashmap_has(kargs, arg);
}

