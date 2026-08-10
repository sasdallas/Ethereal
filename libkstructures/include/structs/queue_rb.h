/**
 * @file libkstructures/include/structs/queue_rb.h
 * @brief Ringbuffer queue (functions as a pointer queue + object buffer)
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef STRUCTS_QUEUE_RB_H
#define STRUCTS_QUEUE_RB_H

#include <structs/ringbuffer.h>
#include <stdbool.h>

typedef ringbuffer_t* queue_rb_t;

/* object queues are queues designed for specific objects. regular queue_push */
/* compared to normal queues, they require no allocations and no lifetime */
typedef struct queue_obj {
    size_t osize;
    ringbuffer_t *rb;
} queue_obj_t;

#define QUEUE_DEFAULT_SIZE      64

#define QUEUE_OBJ_INIT(q, objsize, qsize) ({ (q)->osize = (objsize); (q)->rb = ringbuffer_create((qsize) * (objsize)); })
#define QUEUE_RB_INIT(q, qsize) (*(q) = ringbuffer_create((qsize) * sizeof(void*)))
#define QUEUE_RB_DEINIT(q) (ringbuffer_destroy(*(q)))

static inline bool queue_rb_empty(queue_rb_t *rb_queue) {
    return ringbuffer_remaining_read(*rb_queue) < sizeof(void*);
}

static inline bool queue_rb_space(queue_rb_t *rb_queue) {
    return ringbuffer_remaining_write(*rb_queue) >= sizeof(void*);
}

static inline void queue_rb_push(queue_rb_t *rb_queue, void *data) {
    ringbuffer_write(*rb_queue, (char*)&data, sizeof(void*));
}

static inline int queue_rb_pop(queue_rb_t *rb_queue, void **data) {
    if (queue_rb_empty(rb_queue)) return -1;
    ringbuffer_read(*rb_queue, (char*)data, sizeof(void*));
    return 0;
}

static inline int queue_rb_peek(queue_rb_t *rb_queue, void **data) {
    if (queue_rb_empty(rb_queue)) return -1;
    ringbuffer_peek(*rb_queue, (char*)data, sizeof(void*));
    return 0;
}

static inline bool queue_obj_empty(queue_obj_t *ob_queue) {
    return ringbuffer_remaining_read(ob_queue->rb) < ob_queue->osize;
}

static inline bool queue_obj_space(queue_obj_t *ob_queue) {
    return ringbuffer_remaining_write(ob_queue->rb) >= ob_queue->osize;
}

static inline void queue_obj_push(queue_obj_t *ob_queue, void *data) {
    ringbuffer_write(ob_queue->rb, (char*)data, ob_queue->osize);
}

static inline int queue_obj_pop(queue_obj_t *ob_queue, void *data) {
    if (queue_obj_empty(ob_queue)) return -1;
    ringbuffer_read(ob_queue->rb, (char*)data, ob_queue->osize);
    return 0;
}

static inline int queue_obj_peek(queue_obj_t *ob_queue, void *data) {
    if (queue_obj_empty(ob_queue)) return -1;
    ringbuffer_peek(ob_queue->rb, (char*)data, ob_queue->osize);
    return 0;
}

#endif
