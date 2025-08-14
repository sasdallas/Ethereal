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
#include <kernel/mem/alloc.h>
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

    // kargs is to be redone.
    return;
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

