/**
 * @file libkstructures/ringbuffer/ringbuffer.c
 * @brief Ringbuffer
 * 
 * Ringbuffers are newer versions of the old circbuf system which do not block
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <structs/ringbuffer.h>
#include <stdlib.h>
#include <string.h>


/**
 * @brief Create ringbuffer
 * @param size The size of the ringbuffer to create
 */
ringbuffer_t *ringbuffer_create(size_t size) {
    ringbuffer_t *ringbuffer = malloc(sizeof(ringbuffer_t));
    ringbuffer->buffer_size = size;
    ringbuffer->buffer = malloc(size);
    ringbuffer->head = 0;
    ringbuffer->tail = 0;
    return ringbuffer;
}

/**
 * @brief Write to ringbuffer
 * @param ringbuffer The ringbuffer to write to
 * @param buffer The buffer to write in
 * @param count The count to write
 * @returns The amount of bytes written
 */
ssize_t ringbuffer_write(ringbuffer_t *ringbuffer, char *buffer, size_t count) {
    size_t avail = (ringbuffer->buffer_size - (ringbuffer->tail - ringbuffer->head));
    if (!avail) return 0;
    if (count > avail) count = avail;
    size_t offset = ringbuffer->tail % ringbuffer->buffer_size;

    size_t l1 = ringbuffer->buffer_size - offset;
    if (l1 > count) l1 = count;

    memcpy(ringbuffer->buffer + offset, buffer, l1);

    if (l1 == count) {
        ringbuffer->tail = (ringbuffer->tail + count) ;
        return count;
    }

    memcpy(ringbuffer->buffer, buffer + l1, count - l1);
    ringbuffer->tail = (ringbuffer->tail + count) % ringbuffer->buffer_size;
    return count;
}

/**
 * @brief Read from ringbuffer
 * @param ringbuffer The ringbuffer to read from
 * @param buffer The buffer to read into
 * @param count The count to read in
 */
ssize_t ringbuffer_read(ringbuffer_t *ringbuffer, char *buffer, size_t count) {
    size_t avail = ringbuffer->tail - ringbuffer->head;
    if (!avail) return 0;
    if (count > avail) count = avail;

    size_t l1 = (ringbuffer->buffer_size - 1) - ringbuffer->head + 1;
    if (l1 > count) l1 = count;

    size_t off = (ringbuffer->head % ringbuffer->buffer_size);
    memcpy(buffer, ringbuffer->buffer + off, l1);

    if (l1 == count) {
        ringbuffer->head = (ringbuffer->head + count);
        return count;
    }

    memcpy(buffer + l1, ringbuffer->buffer, count - l1);
    ringbuffer->head = ringbuffer->head + count;
    return count;
}

/**
 * @brief Returns the amount of data remaining to read
 * @param ringbuffer The ringbuffer
 */
size_t ringbuffer_remaining_read(ringbuffer_t *ringbuffer) {
    return (ringbuffer->tail - ringbuffer->head);
}

/**
 * @brief Returns the amount of data remaining to write
 * @param ringbuffer The ringbuffer
 */
size_t ringbuffer_remaining_write(ringbuffer_t *ringbuffer) {
    return ringbuffer->buffer_size - (ringbuffer->tail - ringbuffer->head);
}

/**
 * @brief Destroy ringbuffer
 * @param ringbuffer The ringbuffer to destroy
 */
void ringbuffer_destroy(ringbuffer_t *ringbuffer) {
    free(ringbuffer->buffer);
    free(ringbuffer);
}