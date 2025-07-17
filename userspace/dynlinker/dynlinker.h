/**
 * @file userspace/dynlinker/dynlinker.h
 * @brief Ethereal dynamic linker
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _DYNLINKER_H
#define _DYNLINKER_H

/**** INCLUDES ****/
#include <kernel/loader/elf.h>
#include <structs/list.h>
#include <structs/hashmap.h>
#include <stdio.h>

/**** DEFINITIONS ****/

#define LINKER_SYMBOL_COUNT     3

/**** TYPES ****/

typedef struct elf_symbol {
    char *name;
    void *addr;
} elf_symbol_t;

typedef struct elf_dynamic {
    void *strtab;               // Dynamic string table

    void *init;                 // INIT
    void *fini;                 // FINI

    void *init_array;           // INIT_ARRAY
    size_t init_arraysz;        // INIT_ARRAYSZ
    void *fini_array;           // FINI_ARRAY
    size_t fini_arraysz;        // FINI_ARRAYSZ
    
    void *hash;                 // HASH

    size_t symtab_sz;           // SYMTAB SIZE (located in HASH)
    void *symtab;               // SYMTAB

    void *rela;                 // RELA
    size_t relaent;             // RELA ENT

    void *jmprel;               // JMPREL

} elf_dynamic_t;

typedef struct elf_obj {
    char *filename;             // The name of the file to load
    FILE *f;                    // File
    void *buffer;               // Buffer in memory
    
    void *base;                 // Base load address (only applies to dynamic libraries)
    size_t size;                // Size (in memory)

    void *dynamic;              // The dynamic symbol table location
    elf_dynamic_t dyntab;       // Dynamic table parsed

    list_t *dependencies;       // Dependencies (in string format) to load
} elf_obj_t;

/**** VARIABLES ****/

extern hashmap_t *__linker_symbol_table;
extern list_t *__linker_libraries;
extern int __linker_debug;
extern const elf_symbol_t __linker_symbols[LINKER_SYMBOL_COUNT];


/**** MACROS ****/

#define LD_DEBUG(...) if (__linker_debug) fprintf(stderr, "ld.so: " __VA_ARGS__)

/**** FUNCTIONS ****/

/**
 * @brief Get an ELF object from a file
 * @param filename The filename to load
 * @returns ELF object or NULL
 */
elf_obj_t *elf_get(char *filename);

/**
 * @brief Load the PHDRs of an ELF file to a designated load address
 * @param obj The object to load
 * @returns 0 on success
 */
int elf_load(elf_obj_t *obj);

/**
 * @brief Perform DT_HASH
 * @param name The name
 */
uint32_t elf_hash(char *name);

/**
 * @brief Parse dynamic table to get all dependencies
 * @param obj The object to parse the table of
 * @returns 0 on success
 */
int elf_dynamic(elf_obj_t *obj);

/** 
 * @brief Apply ELF relocations
 * @param obj The object to relocate
 * @returns 0 on success
 */
int elf_relocate(elf_obj_t *obj);

/**
 * @brief Try to find a symbol in a dynamic library
 * @param lib The dynamic library to search
 * @param name The symbol to look up
 * @returns ELF symbol or NULL if it could not be funod
 */
Elf64_Sym *elf_lookupFromLibrary(elf_obj_t *obj, char *name);

#endif