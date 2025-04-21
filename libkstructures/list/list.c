/**
 * @file libkstructures/list/list.c
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

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <structs/list.h>
#include <kernel/mem/alloc.h>

/**
 * @brief Create a new list
 * @param name Optional name for debugging
 */
list_t *list_create(char *name) {
    list_t *list = kmalloc(sizeof(list_t));
    list->name = name;
    list->head = NULL;
    list->tail = NULL;
    list->length = 0;
    return list;
}

/**
 * @brief Destroys a list and its contents.
 * @param list The list to destroy
 * @param free_values Free the values in the nodes. If false, the nodes will be freed but their values will not be.
 */
void list_destroy(list_t *list, bool free_values) {
    if (!list) return;

    // Start iterating
    node_t *node = list->head;
    while (node) {
        node_t *next = node->next;
        if (free_values) kfree(node->value);
        kfree(node);
        node = next;
    }

    // Free the list
    kfree(list);
}


/**
 * @brief Append a node to the list
 * @param list The list to append to
 * @param node The node to append
 */
void list_append_node(list_t *list, node_t *node) {
    node->owner = (void*)list;

    if (list->tail) {
        node_t *last = list->tail;
        last->next = node;
        node->prev = last;
        node->next = NULL;  
    } else {
        // Fresh list
        node->next = NULL;
        node->prev = NULL;
        list->head = node;
    }

    list->tail = node;
    list->length++;
}

/**
 * @brief Append an item to the list
 * @param list The list to append to
 * @param item The item to append
 */
void list_append(list_t *list, void *item) {
    node_t *node = kmalloc(sizeof(node_t));
    node->value = item;
    list_append_node(list, node);
}

/**
 * @brief Append a node after another node
 * @param list The list to append to
 * @param append_to The node that will be appended to (leave as NULL and it will be inserted at the beginning of the list)
 * @param node The node to append
 */
void list_append_node_after(list_t *list, node_t *append_to, node_t *node) {
    node->owner = (void*)list;

    if (append_to == NULL) {
        // Append to the beginning of the list
        node->next = list->head;
        list->head->prev = node;
        list->head = node;
        list->length++;
        return;
    }

    if (append_to == list->tail) {
        list->tail = node; // Set the tail up
    } else {
        append_to->next->prev = node;
        node->next = append_to->next;
    }

    node->prev = append_to;
    append_to->next = node;
    list->length++;
}

/**
 * @brief Append an item after another node
 * @param list The list to append to
 * @param append_to The node that will be appended to (leave as NULL and it will be inserted at the beginning of the list)
 * @param item The item to append
 */
void list_append_after(list_t *list, node_t *append_to, void *item) {
    node_t *node = kmalloc(sizeof(node_t));
    node->value = item;
    list_append_node_after(list, append_to, node);
}


/**
 * @brief Append a node before another node
 * @param list The list to append to
 * @param append_before The node to append before (leave as NULL and it will be inserted at the end of the list)
 * @param node The node to append
 */
void list_append_node_before(list_t *list, node_t *append_before, node_t *node) {
    node->owner = (void*)list;

    if (append_before == NULL) {
        // Append to the end of the list.
        node->next = NULL;
        node->prev = list->tail;
        list->tail->next = node;
        list->tail = node;
        list->length++;
        return;
    }

    // If we're trying to append before the beginning of the list, handle that.
    if (append_before == list->head) {
        list->head = node; 
    } else {
        append_before->prev->next = node;
        node->prev = append_before->prev;
    }

    node->next = append_before;
    append_before->prev = node;
    list->length++;
}  

/**
 * @brief Append an item before another node
 * @param list The list to append to
 * @param append_before The node to append before (leave as NULL and it will be inserted at the end of the list)
 * @param item The item to append
 */
void list_append_before(list_t *list, node_t *append_before, void *item) {
    node_t *node = kmalloc(sizeof(node_t));
    node->value = item;
    list_append_node_before(list, append_before, node);
}

/**
 * @brief Find an item in the list
 * @param list The list to check
 * @param item The item to check for
 * @returns The node, or NULL if it couldn't be found.
 */
node_t *list_find(list_t *list, void *item) {
    node_t *node = list->head;
    while (node) {
        if (node->value == item) return node;
        node = node->next;
    }

    return NULL;
}  

/**
 * @brief Delete a node from the list
 * @param list The list to delete from
 * @param node The node to delete
 * @note This does not free the node structure, if you need to use it.
 */
void list_delete(list_t *list, node_t *node) {
    if (node == list->head) list->head = node->next;
    if (node == list->tail) list->tail = node->prev;
    if (node->next) node->next->prev = node->prev;
    if (node->prev) node->prev->next = node->next;

    node->prev = NULL;
    node->next = NULL;
    node->owner = NULL;
    list->length--;
}

/**
 * @brief Delete a node at a specific index from the list
 * @param list The list to delete from
 * @param index The index of the node
 */
void list_delete_index(list_t *list, size_t index) {
    if (index > list->length) return;

    // No faster way to do this
    size_t i = 0;
    node_t *node = list->head;
    while (node && i < index) {
        node = node->next;
        i++;
    }

    if (i == index) {
        list_delete(list, node);
    }
}

/**
 * @brief Pop the last node off the list and return it
 * @param list The list to pop off
 */
node_t *list_pop(list_t *list) {
    if (!list || !list->tail) return NULL;
    node_t *out = list->tail;
    list_delete(list, out);
    return out;
}

/**
 * @brief Pop the first node off the list and return it
 * @param list The list to pop left on
 */
node_t *list_popleft(list_t *list) {
    if (!list || !list->head) return NULL;
    node_t *out = list->head;
    list_delete(list, out);
    return out;
}