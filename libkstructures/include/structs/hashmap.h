/**
 * @file libkstructures/include/structs/hashmap.h
 * @brief Hashmap header
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef STRUCTS_HASHMAP_H
#define STRUCTS_HASHMAP_H

/**** INCLUDES ****/
#include <stdint.h>
#include <structs/list.h>

/**** DEFINITIONS ****/

#define HASHMAP_PTR         0   // All keys in the hashmap are pointers and can be hashed
#define HASHMAP_INT         1   // All keys in the hashmap are integers and cannot be hashed

/**** TYPES ****/

// Node used in hashmap
typedef struct _hashmap_node {
    char *key;
    void *value;
    struct _hashmap_node *next;
} hashmap_node_t;

// Hashmap
typedef struct _hashmap {
    int type;                   // Type (HASHMAP_PTR or HASHMAP_INT)
    char *name;                 // Optional name
    size_t size;                // Size of the hashmap
    hashmap_node_t **entries;   // List of nodes in the hashmap (sourced from old Ethereal Operating System hashmap impl.)
                                // This is a sort of quick access system
} hashmap_t;

/**** FUNCTIONS ****/

/**
 * @brief Create a new hashmap
 * @param name Optional name parameter. This is for your bookkeeping.
 * @param size The amount of entries in the hashmap.
 */
hashmap_t *hashmap_create(char *name, size_t size);

/**
 * @brief Create a new integer hashmap
 * @param name Optional name parameter. This is for your bookkeeping.
 * @param size The amount of entries in the hashmap.
 * @note An integer hashmap means that no keys in the hashmap will ever be touched in memory
 */
hashmap_t *hashmap_create_int(char *name, size_t size);

/**
 * @brief Set value in hashmap
 * @param hashmap The hashmap to set the value in
 * @param key The key to use
 * @param value The value to set
 */
void hashmap_set(hashmap_t *hashmap, void *key, void *value);

/**
 * @brief Find an entry within a hashmap
 * @param hashmap The hashmap to search
 * @param key The key to look for
 * @returns The value set by @c hashmap_set
 */
void *hashmap_get(hashmap_t *hashmap, void *key);

/**
 * @brief Remove something from a hashmap
 * @param hashmap The hashmap to remove an item from
 * @param key The key to remove
 * @returns Either NULL if the key couldn't be found or the value that was there
 */
void *hashmap_remove(hashmap_t *hashmap, void *key);

/**
 * @brief Returns whether a hashmap has a key
 * @param hashmap The hashmap to check
 * @param key The key to check for
 */
int hashmap_has(hashmap_t *hashmap, void *key);

/**
 * @brief Returns a list of hashmap keys
 * @param hashmap The hashmap to return a list of keys for
 */
list_t *hashmap_keys(hashmap_t *hashmap);

/**
 * @brief Returns a list of hashmap values
 * @param hashmap The hashmap to return a list of values for
 */
list_t *hashmap_values(hashmap_t *hashmap);

/**
 * @brief Free a hashmap (note: does not free values)
 * @param hashmap The hashmap to free
 */
void hashmap_free(hashmap_t *hashmap);

#endif