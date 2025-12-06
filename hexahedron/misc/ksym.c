/**
 * @file hexahedron/misc/ksym.c
 * @brief Kernel symbol loader
 * 
 * This uses a symbol file produced by nm
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */


#include <kernel/misc/ksym.h>

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <kernel/debug.h>
#include <kernel/mm/alloc.h>
#include <structs/hashmap.h>

/* Symbol hashmap */
hashmap_t *ksym_hashmap = NULL;

/**
 * @brief Bind a symbol to the hashmap
 * @param symname The name of the symbol
 * @param address The address of the symbol
 */
void ksym_bind_symbol(char *symname, uintptr_t address) {
    if (!hashmap_has(ksym_hashmap, symname)) {
        hashmap_set(ksym_hashmap, symname, (void*)address);
    }
    // hashmap_set(ksym_hashmap, (void*)symname, (void*)address);
}

/**
 * @brief Initialize the kernel symbol map
 * @param file The symbol map file
 * @returns Number of symbols loaded or error code
 */
int ksym_load(fs_node_t *file) {
    if (!file) return -EINVAL;
    if (ksym_hashmap) return -EALREADY;
    ksym_hashmap = hashmap_create("ksym", 20);
    
    // Read the contents of the file
    uint8_t *symbuf = kmalloc(file->length);
    memset(symbuf, 0, file->length);
    ssize_t b = fs_read(file, 0, file->length, symbuf);

    if ((size_t)b != file->length) {
        kfree(symbuf);
        return -EINVAL;
    }

    // The nm tool constructs a symbol map like so:
    // Column 1 - Address of the symbol
    // Column 2 - Type of the symbol (not defined)
    // Column 3 - Name of the symbol

    char *save;
    char *save2;
    char *token = strtok_r((char*)symbuf, "\n", &save);
    int symbols = 0;

    while (token) {
        token = strtok_r(NULL, "\n", &save);

        // If token is NULL the map is done populating
        if (token == NULL) {
            break;
        }

        // Duplicate the token, we'll parse it for spaces
        char *token_copy = strdup(token); // strtok_r will trash its first content
        char *token2 = (char*)token_copy; 

        char *address = strtok_r(token2, " ", &save2);
        if (!address) goto _next_token;
        char *type = strtok_r(NULL, " ", &save2);
        if (!type) goto _next_token;
        char *symname = strtok_r(NULL, " ", &save2);
        if (!symname) goto _next_token;

        // hack for architecture address length
        uintptr_t addr;
        if (sizeof(uintptr_t) == 8) {
            addr = strtoull(address, NULL, 16);
        } else {
            addr = strtol(address, NULL, 16);
        }

        ksym_bind_symbol(symname, addr);
        symbols++;

    _next_token:
        kfree(token_copy);
    }


    kfree(symbuf);
    return symbols;
}

/**
 * @brief Resolve a symbol name to a symbol address
 * @param name The name of the symbol to resolve
 * @returns The address or NULL
 */
uintptr_t ksym_resolve(char *symname) {
    return (uintptr_t)hashmap_get(ksym_hashmap, symname);
}

/**
 * @brief Resolve an address to the best matching symbol
 * @param address The address of the symbol
 * @param name Output name of the symbol
 * @returns The address of the symbol
 */
uintptr_t ksym_find_best_symbol(uintptr_t address, char **name) {
    if (ksym_hashmap == NULL) {
        return 0x0;
    }

    uintptr_t best_match = 0x0;
    for (size_t i = 0; i < ksym_hashmap->size; i++) {
        hashmap_node_t *node = ksym_hashmap->entries[i];
        
        // Follow chain
        while (node)  {
            uintptr_t sym_addr = (uintptr_t)node->value;
            if (sym_addr <= address && sym_addr > best_match) {
                best_match = sym_addr;
                *name = node->key;
            }

            node = node->next;
        }
    }

    return best_match;
}