/**
 * @file libkstructures/include/structs/circbuf.h
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

#ifndef STRUCTS_CIRCBUF_H
#define STRUCTS_CIRCBUF_H

/**** INCLUDES ****/
#include <stdint.h>
#include <stddef.h>
#include <kernel/misc/spinlock.h>

/**** TYPES ****/

/**
 * @brief A circular buffer
 */
typedef struct circbuf {
    char *name;             // Optional name
    spinlock_t *lock;       // Lock
    uint8_t *buffer;        // Allocated buffer
    size_t buffer_size;     // Size of the buffer
    volatile int head;      // Head of the buffer
    volatile int tail;      // Tail of the buffer
} circbuf_t;

/**** FUNCTIONS ****/

/**
 * @brief Create a new circular buffer
 * @param name Optional name of the circular buffer
 * @param size Size of the circular buffer
 * @returns A pointer to the circular buffer
 */
circbuf_t *circbuf_create(char *name, size_t size);

/**
 * @brief Get some data from a circular buffer
 * @param circbuf The buffer to get from
 * @param size The amount of data to get
 * @param buffer The output buffer
 * @returns 0 on success, anything else probably means the buffer is empty
 */
int circbuf_read(circbuf_t *circbuf, size_t size, uint8_t *buffer);

/**
 * @brief Write some data to a circular buffer
 * @param circbuf The buffer to write to
 * @param size The amount of data to write
 * @param buffer The input buffer
 * @returns 0 on success
 */
int circbuf_write(circbuf_t *circbuf, size_t size, uint8_t *buffer);

/**
 * @brief Returns whether a circular buffer has any content available
 * @param circbuf The circular buffer
 */
int circbuf_available(circbuf_t *circbuf);

/**
 * @brief Destroy a circular buffer
 * @param circbuf The circular buffer to destroy
 */
void circbuf_destroy(circbuf_t *circbuf);

#endif