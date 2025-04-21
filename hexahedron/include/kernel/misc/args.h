/**
 * @file hexahedron/include/kernel/misc/args.h
 * @brief Kernel arguments handler
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_MISC_ARGS_H
#define KERNEL_MISC_ARGS_H

/**** INCLUDES ****/
#include <stdint.h>

/**** FUNCTIONS ****/

/**
 * @brief Initialize the argument parser
 * @param args A string pointing to the arguments
 */
void kargs_init(char *args);

/**
 * @brief Get the argument value for a key
 * @param arg The argument to get the value for
 * @returns NULL on not found or the value
 */
char *kargs_get(char *arg);

/**
 * @brief Returns whether the arguments list has an argument
 * @param arg The argument to check for
 */
int kargs_has(char *arg);

#endif
