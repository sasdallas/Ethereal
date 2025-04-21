/**
 * @file libkstructures/include/structs/list.h
 * @brief Generic list implementation
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef STRUCTS_LIST_H
#define STRUCTS_LIST_H

/**** INCLUDES ****/
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <structs/node.h>

/**** TYPES ****/

// List structure
typedef struct _list {
    char    *name;          // Optional name for debugging
    node_t  *head;          // Starting node of the list
    node_t  *tail;          // Ending node of the list    
    size_t  length;         // Length of the list, in nodes
} list_t;

/**** DEFINITIONS ****/

#define foreach(i, list) for (node_t *i = list->head; i != NULL; i = i->next)

/**** FUNCTIONS ****/

/**
 * @brief Create a new list
 * @param name Optional name for debugging
 */
list_t *list_create(char *name);

/**
 * @brief Destroys a list and its contents.
 * @param list The list to destroy
 * @param free_values Free the values in the nodes. If false, the nodes will be freed but their values will not be.
 */
void list_destroy(list_t *list, bool free_values);

/**
 * @brief Append an item to the list
 * @param list The list to append to
 * @param item The item to append
 */
void list_append(list_t *list, void *item);

/**
 * @brief Append an item after another node
 * @param list The list to append to
 * @param append_to The node that will be appended to (leave as NULL and it will be inserted at the beginning of the list)
 * @param item The item to append
 */
void list_append_after(list_t *list, node_t *append_to, void *item);

/**
 * @brief Append an item before another node
 * @param list The list to append to
 * @param append_before The node to append before (leave as NULL and it will be inserted at the end of the list)
 * @param item The item to append
 */
void list_append_before(list_t *list, node_t *append_before, void *item);

/**
 * @brief Append a node to the list
 * @param list The list to append to
 * @param node The node to append
 */
void list_append_node(list_t *list, node_t *node);

/**
 * @brief Append a node after another node
 * @param list The list to append to
 * @param append_to The node that will be appended to (leave as NULL and it will be inserted at the beginning of the list)
 * @param node The node to append
 */
void list_append_node_after(list_t *list, node_t *append_to, node_t *node);

/**
 * @brief Append a node before another node
 * @param list The list to append to
 * @param append_before The node to append before (leave as NULL and it will be inserted at the end of the list)
 * @param node The node to append
 */
void list_append_node_before(list_t *list, node_t *append_before, node_t *node);

/**
 * @brief Find an item in the list
 * @param list The list to check
 * @param item The item to check for
 * @returns The node, or NULL if it couldn't be found.
 */
node_t *list_find(list_t *list, void *item);

/**
 * @brief Delete a node from the list
 * @param list The list to delete from
 * @param node The node to delete
 * @note This does not free the node structure, if you need to use it.
 */
void list_delete(list_t *list, node_t *node);

/**
 * @brief Delete a node at a specific index from the list
 * @param list The list to delete from
 * @param index The index of the node
 */
void list_delete_index(list_t *list, size_t index);

/**
 * @brief Pop the last node off the list and return it
 * @param list The list to pop off
 */
node_t *list_pop(list_t *list);

/**
 * @brief Pop the first node off the list and return it
 * @param list The list to pop left on
 */
node_t *list_popleft(list_t *list);

#endif

