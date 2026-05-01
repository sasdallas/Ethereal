/**
 * @file libkstructures/include/structs/queue_rb.h
 * @brief Ringbuffer queue
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

#define QUEUE_DEFAULT_SIZE      64

#define QUEUE_RB_INIT(q, qsize) (*(q) = ringbuffer_create(qsize * sizeof(void*)))
#define QUEUE_RB_DEINIT(q) (ringbuffer_destroy(*(q)))

static inline bool queue_rb_empty(queue_rb_t *rb_queue) {
    return ringbuffer_remaining_read(*rb_queue) < sizeof(void*);
}

static inline void queue_rb_push(queue_rb_t *rb_queue, void *data) {
    ringbuffer_write(*rb_queue, (char*)&data, sizeof(void*));
}

static inline int queue_rb_pop(queue_rb_t *rb_queue, void **data) {
    if (queue_rb_empty(rb_queue)) return -1;
    ringbuffer_read(*rb_queue, (char*)data, sizeof(void*));
    return 0;
}

#endif