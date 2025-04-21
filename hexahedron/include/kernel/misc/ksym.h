/**
 * @file hexahedron/include/kernel/misc/ksym.h
 * @brief Kernel symbol loader
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_MISC_KSYM_H
#define KERNEL_MISC_KSYM_H

/**** INCLUDES ****/
#include <stdint.h>
#include <kernel/fs/vfs.h>

/**** TYPES ****/


/**** FUNCTIONS ****/

/**
 * @brief Initialize the kernel symbol map
 * @param file The symbol map file
 * @returns Number of symbols loaded or error code
 */
int ksym_load(fs_node_t *file);

/**
 * @brief Resolve a symbol name to a symbol address
 * @param name The name of the symbol to resolve
 * @returns The address or NULL
 */
uintptr_t ksym_resolve(char *symname);

/**
 * @brief Resolve an address to the best matching symbol
 * @param address The address of the symbol
 * @param name Output name of the symbol
 * @returns The address of the symbol
 */
uintptr_t ksym_find_best_symbol(uintptr_t address, char **name);

#endif