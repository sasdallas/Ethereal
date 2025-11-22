/**
 * @file libkstructures/include/structs/queue.h
 * @brief Queue
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef STRUCTS_QUEUE_H
#define STRUCTS_QUEUE_H

/**** INCLUDES ****/
#include <stdlib.h>

/**** TYPES ****/

typedef struct queue_node {
    struct queue_node *next;
    void *data;
} queue_node_t;

typedef struct queue {
    queue_node_t *head;
    queue_node_t *tail;
    size_t size;
} queue_t; 

/**** MACROS ****/
#define QUEUE_NODE_INITIALIZE(n, v)  { ((n))->val = (v); }

/**** FUNCTIONS ****/

static inline queue_t *queue_create() {
    return calloc(1, sizeof(queue_t));
}

static inline queue_node_t *queue_node_create(void *v) {
    queue_node_t *n = malloc(sizeof(queue_node_t));
    n->data = v;
    return n;
}

static inline void queue_push_node(queue_t *queue, queue_node_t *n) {
    if (!queue->size) { queue->head = queue->tail = n; queue->size++; return; }
    queue->tail->next = n;
    queue->tail = n;
    queue->size++;
    n->next = NULL;
}

static inline void queue_push(queue_t *queue, void *d) {
    queue_node_t *n = malloc(sizeof(queue_node_t));
    n->data = d;
    n->next = NULL;
    if (!queue->size) { queue->head = queue->tail = n; queue->size++; return; }

    queue->tail->next = n;
    queue->tail = n;
    queue->size++;
}

static inline void *queue_pop(queue_t *queue) {
    if (!queue->size) return NULL;

    queue_node_t *n = queue->head;
    queue->head = queue->head->next;
    queue->size--;

    if (!queue->size) { queue->head = NULL; queue->tail = NULL; }

    void *d = n->data;
    free(n);
    return d;
}

static inline int queue_empty(queue_t *queue) {
    return !queue->size;
}


#endif