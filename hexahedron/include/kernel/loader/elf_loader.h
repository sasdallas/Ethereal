/**
 * @file hexahedron/include/kernel/loader/elf_loader.h
 * @brief ELF loader header
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef KERNEL_LOADER_ELF_LOADER_H
#define KERNEL_LOADER_ELF_LOADER_H

/**** INCLUDES ****/
#include <stdint.h>
#include <kernel/fs/vfs.h>

/**** DEFINITIONS ****/

#define ELF_KERNEL  0   // Load an ELF file with full kernel access (drivers)
#define ELF_USER    1   // Load an ELF file with usermode access (programs)
#define ELF_DRIVER  2   // Equivalent to ELF_KERNEL but load the program with allocations in driver memory space

#define ELF_FAIL    (uintptr_t)1       // Ugliest value ever...

// The type of file we're trying to load
#define ELF_EXEC        0   // Executable object files
#define ELF_RELOC       1   // Relocatable object files
#define ELF_DYNAMIC     2   // Dynamically linked object files
#define ELF_ANY         3   // It doesn't matter what file we're looking for

/**** TYPES ****/

typedef struct elf_dynamic_info {
    uintptr_t at_entry;         // AT_ENTRY
    uintptr_t at_phdr;          // AT_PHDR
    uintptr_t at_phnum;         // AT_PHNUM
    uintptr_t at_phent;         // AT_PHENT
} elf_dynamic_info_t;

/**** FUNCTIONS ****/

/**
 * @brief Find a specific symbol by name and get its value
 * @param ehdr_address The address of the EHDR (as elf64/elf32 could be in use)
 * @param name The name of the symbol
 * @returns A pointer to the symbol or NULL
 * 
 * @warning Make sure you've initialized the file!
 */
uintptr_t elf_findSymbol(uintptr_t ehdr_address, char *name);

/**
 * @brief Check a file to make sure it is a valid ELF file
 * @param file The file to check
 * @param type The type of file to look for
 * @returns 1 on valid ELF file, 0 on invalid
 */
int elf_check(fs_node_t *file, int type);

/**
 * @brief Get the entrypoint of an executable file
 * @param ehdr_address The address of the EHDR (as elf64/elf32 could be in use)
 * @returns A pointer to the start or NULL
 */
uintptr_t elf_getEntrypoint(uintptr_t ehdr_address);

/**
 * @brief Load an ELF file fully
 * @param fbuf The buffer to load the file from
 * @param flags The flags to use (ELF_KERNEL or ELF_USER)
 * @returns A pointer to the file that can be passed to @c elf_getEntrypoint or @c elf_findSymbol - or NULL if there was an error. 
 */
uintptr_t elf_loadBuffer(uint8_t *fbuf, int flags);

/**
 * @brief Load an ELF file into memory
 * @param file The file to load into memory
 * @param flags The flags to use (ELF_KERNEL or ELF_USER)
 * @returns A pointer to the file that can be passed to @c elf_getEntrypoint or @c elf_findSymbol - or NULL if there was an error. 
 */
uintptr_t elf_load(fs_node_t *node, int flags);

/**
 * @brief Cleanup an ELF file after it has finished executing
 * @param elf_address The address given by @c elf_load or another loading function
 * @returns 0 on success, anything else is a failure
 * 
 * @note REMEMBER TO FREE ELF BUFFER WHEN FINISHED!
 */
int elf_cleanup(uintptr_t elf_address);

/**
 * @brief Get the end of an ELF binary (for heap locations)
 * @param elf_address Address given by @c elf_load
 * @returns Pointer to heap placement location (aligned)
 */
uintptr_t elf_getHeapLocation(uintptr_t elf_address);

/**
 * @brief Populate a process image object
 * @param elf_address Address given by @c elf_load
 */
void elf_createImage(uintptr_t elf_address);

/**
 * @brief Detect and find the interpreter for a dynamically linked file
 * @param file The file to get the interpreter for
 * @returns Interpreter string on success or NULL on failure
 * @note Free this yourself
 */
char *elf_getInterpreter(fs_node_t *file);

/**
 * @brief Load dynamic ELF file
 * @param file The file to load dynamically
 * @param info Output @c elf_dynamic_info_t
 * @returns 0 on success
 */
int elf_loadDynamicELF(fs_node_t *file, elf_dynamic_info_t *info);

#endif