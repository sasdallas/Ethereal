/**
 * @file hexahedron/include/kernel/loader/binfmt.h
 * @brief Binayr format 
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef KERNEL_LOADER_BINFMT_H
#define KERNEL_LOADER_BINFMT_H

/**** INCLUDES ****/
#include <stdint.h>
#include <stddef.h>
#include <kernel/fs/vfs.h>

/**** DEFINITIONS ****/

// Max amount of binfmt entries
#define BINFMT_MAX          10

// Ugh..
#define BINFMT_BYTE_MAX     10  

/**** TYPES ****/

/**
 * @brief binfmt execution
 * @param path Full path of the file
 * @param file The file to execute
 * @param argc The argument count
 * @param argv The argument list
 * @param envp The environment variables pointer
 * @returns Error code (jump to process if successful)
 */
typedef int (*binfmt_load_t)(char *path, fs_node_t *file, int argc, char **argv, char **envp);

typedef struct binfmt_entry {
    char *name;                     // Optional name
    binfmt_load_t load;             // Load function
    size_t match_count;             // Amount of bytes to match
    uint8_t bytes[BINFMT_BYTE_MAX]; // Bytes to match
} binfmt_entry_t;

/**** FUNCTIONS ****/

/**
 * @brief Register a new entry in the binfmt table
 * @param entry The binary entry to register
 */
int binfmt_register(binfmt_entry_t entry);

/**
 * @brief Start execution of a process or return an error code
 * @param path Full path of the file
 * @param file The file to execute
 * @param argc The argument count
 * @param argv The argument list
 * @param envp The environment variables pointer
 * @returns Error code
 */
int binfmt_exec(char *path, fs_node_t *file, int argc, char **argv, char **envp);

#endif