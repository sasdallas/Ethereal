/**
 * @file hexahedron/misc/args.c
 * @brief Kernel arguments handler
 * 
 * This handles the kernel arguments in a hashmap organization. It parses
 * the string given to @c kargs_init
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <kernel/misc/args.h>
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

    // Create hashmap
    kargs = hashmap_create("kargs", 10);
    char *x = args;

    // Start iterating
    while (x) {
        // Handle spaces in the beginning of the arguments prmpt
        while (*x && *x == ' ') x++;

        // No x?
        if (!(*x)) break;


        // We should now be positioned at the beginning argument.
        char *arg = x;
        char *arg_value = NULL; // Handled after
        while (*x && *x != ' ' && *x != '=') x++;

        // We're at the end of the argument. Is there an '='?
        if (*x == '=') {
            // Yes, parse value.
            *x++ = '\0'; // We have to set zeroes for the key.

            // Is the value quoted?
            if (*x == '\"') {
                // Yes, we have some work to do.
                // The reason this actually involves work is mainly just the escape sequences.
                x++;
                arg_value = x;
                char *y = x; // Second pointer that we can mess with

                
                while (*x && *x != '\"') {
                    if (*x == '\\') {
                        // Escape sequence, handle it.
                        x++;
                        switch (*x) {
                            case '\"': *y++ = '"'; x++; break;
                            case '\\': *y++ = '\\'; x++; break;
                            case '\0': 
                                dprintf(WARN, "Failed to parse argument value for argument %s\n", arg);
                                goto _done;
                            default:
                                // I guess they wanted this to not be an escape sequence.
                                *y++ = '\\';
                                *y++ = *x++;
                                break;
                        }
                    } else {
                        // Normal character
                        *y++ = *x++;
                    }
                }


                // All done handling
                if (!x) {
                    dprintf(WARN, "Failed to parse argument value for argument %s\n", arg);
                    goto _done;
                }

                if (*x == '\"') {
                    // Set the ending character
                    *y = '\0';
                    x++;
                }

            } else {
                // No, we don't have to do much work - just construct a string.
                arg_value = x;
                while (*x && *x != ' ') x++;
                if (*x == ' ') {
                    // Set the ending character.
                    *x = '\0';
                    x++;
                }
            }
        } else if (*x == ' ') {
            // No, end of argument. Set the ending byte to prevent arg from overflowing.
            *x++ = '\0';
        }

        // Set argument
        hashmap_set(kargs, arg, (arg_value == NULL) ? NULL : strdup(arg_value));
        dprintf(DEBUG, "Finished parsing argument '%s' with value '%s'\n", arg, (arg_value == NULL) ? "NULL" : arg_value);
    }

_done:
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

