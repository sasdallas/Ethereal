/**
 * @file libkstructures/include/structs/ringbuffer.h
 * @brief Ringbuffer
 * 
 * A ring buffer does not replace a circular buffer. A ring buffer is designed for
 * small parts that do not require locking.
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef STRUCTS_RINGBUFFER_H
#define STRUCTS_RINGBUFFER_H

/**** INCLUDES ****/
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

/**** TYPES ****/
typedef struct ringbuffer {
    int head;
    int tail;    
    size_t buffer_size;
    char *buffer;
} ringbuffer_t;

/**** FUNCTIONS ****/

/**
 * @brief Create ringbuffer
 * @param size The size of the ringbuffer to create
 */
ringbuffer_t *ringbuffer_create(size_t size);

/**
 * @brief Write to ringbuffer
 * @param ringbuffer The ringbuffer to write to
 * @param buffer The buffer to write in
 * @param count The count to write
 * @returns The amount of bytes written
 */
ssize_t ringbuffer_write(ringbuffer_t *ringbuffer, char *buffer, size_t count);

/**
 * @brief Read from ringbuffer
 * @param ringbuffer The ringbuffer to read from
 * @param buffer The buffer to read into
 * @param count The count to read in
 */
ssize_t ringbuffer_read(ringbuffer_t *ringbuffer, char *buffer, size_t count);

/**
 * @brief Returns the amount of data remaining to read
 * @param ringbuffer The ringbuffer
 */
size_t ringbuffer_remaining_read(ringbuffer_t *ringbuffer);

/**
 * @brief Returns the amount of data remaining to write
 * @param ringbuffer The ringbuffer
 */
size_t ringbuffer_remaining_write(ringbuffer_t *ringbuffer);

/**
 * @brief Destroy ringbuffer
 * @param ringbuffer The ringbuffer to destroy
 */
void ringbuffer_destroy(ringbuffer_t *ringbuffer);

#endif