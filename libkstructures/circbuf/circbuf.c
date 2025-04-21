/**
 * @file libkstructures/circbuf/circbuf.c
 * @brief Circular buffer implementation
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <structs/circbuf.h>
#include <kernel/mem/alloc.h>
#include <string.h>

/**
 * @brief Create a new circular buffer
 * @param name Optional name of the circular buffer
 * @param size Size of the circular buffer
 * @returns A pointer to the circular buffer
 */
circbuf_t *circbuf_create(char *name, size_t size) {
    circbuf_t *circbuf = kmalloc(sizeof(circbuf_t));
    memset(circbuf, 0, sizeof(circbuf_t));

    circbuf->name = name;
    circbuf->buffer_size = size;
    circbuf->buffer = kmalloc(size);
    memset(circbuf->buffer, 0, size);
    circbuf->lock = spinlock_create("circular buffer lock");
    circbuf->head = 0;
    circbuf->tail = 0;

    return circbuf;
}

/**
 * @brief Get some data from a circular buffer
 * @param circbuf The buffer to get from
 * @param size The amount of data to get
 * @param buffer The output buffer
 * @returns 0 on success, anything else probably means the buffer is empty
 */
int circbuf_read(circbuf_t *circbuf, size_t size, uint8_t *buffer) {
    if (!circbuf || !buffer) return 1;

    spinlock_acquire(circbuf->lock);

    // Start readin' data
    for (size_t i = 0; i < size; i++) {
        if (circbuf->tail == circbuf->head) {
            // no more content
            // TODO: handle more appropriately? this is weird..
            spinlock_release(circbuf->lock);
            return 1;
        }

        buffer[i] = circbuf->buffer[circbuf->tail];
        circbuf->tail++;
        if ((size_t)circbuf->tail >= circbuf->buffer_size) circbuf->tail = 0;
    }

    spinlock_release(circbuf->lock);

    return 0;
}

/**
 * @brief Write some data to a circular buffer
 * @param circbuf The buffer to write to
 * @param size The amount of data to write
 * @param buffer The input buffer
 * @returns 0 on success
 */
int circbuf_write(circbuf_t *circbuf, size_t size, uint8_t *buffer) {
    if (!circbuf || !buffer) return 1;

    spinlock_acquire(circbuf->lock);

    // Start copyin' data
    for (size_t i = 0; i < size; i++) {
        circbuf->buffer[circbuf->head] = buffer[i];
        circbuf->head++;
        if ((size_t)circbuf->head >= circbuf->buffer_size) circbuf->head = 0;
    }

    spinlock_release(circbuf->lock);

    return 0;
}

/**
 * @brief Returns whether a circular buffer has any content available
 * @param circbuf The circular buffer
 */
int circbuf_available(circbuf_t *circbuf) {
    return !(circbuf->tail == circbuf->head);
}

/**
 * @brief Destroy a circular buffer
 * @param circbuf The circular buffer to destroy
 */
void circbuf_destroy(circbuf_t *circbuf) {
    spinlock_destroy(circbuf->lock);
    kfree(circbuf->buffer);
    kfree(circbuf);
}